#include "event_bus.h"

#include <string.h>

static void nb_event_bus_refresh_free_counts(nb_event_bus_t *bus)
{
    uint8_t free_normal = 0;
    uint8_t free_safety = 0;

    for (uint8_t i = 0; i < NB_EVENT_BUS_MAX_SLOTS; ++i) {
        if (bus->slots[i].in_use) {
            continue;
        }
        if (bus->slots[i].safety_reserved) {
            ++free_safety;
        } else {
            ++free_normal;
        }
    }

    bus->stats.free_normal_slots = free_normal;
    bus->stats.free_safety_slots = free_safety;
    bus->stats.normal_pending = bus->normal_count;
    bus->stats.safety_pending = bus->safety_count;
}

static void nb_event_bus_audit(nb_event_bus_t *bus,
                               const nb_event_t *event,
                               nb_event_audit_action_t action,
                               nb_event_bus_status_t status)
{
    nb_event_audit_entry_t *entry = &bus->audit[bus->audit_head];

    entry->action = action;
    entry->status = status;
    entry->type = event != NULL ? event->type : 0u;
    entry->priority =
        event != NULL ? event->priority : NB_EVENT_PRIORITY_NORMAL;
    entry->timestamp_ms = event != NULL ? event->timestamp_ms : 0u;
    entry->normal_pending = bus->normal_count;
    entry->safety_pending = bus->safety_count;

    bus->audit_head =
        (uint8_t)((bus->audit_head + 1u) % NB_EVENT_BUS_AUDIT_CAPACITY);
    if (bus->audit_count < NB_EVENT_BUS_AUDIT_CAPACITY) {
        ++bus->audit_count;
    }
}

static bool nb_event_bus_queue_push(uint8_t *queue,
                                    uint8_t capacity,
                                    uint8_t *tail,
                                    uint8_t *count,
                                    uint8_t slot_index)
{
    if (*count >= capacity) {
        return false;
    }

    queue[*tail] = slot_index;
    *tail = (uint8_t)((*tail + 1u) % capacity);
    ++(*count);
    return true;
}

static bool nb_event_bus_queue_pop(uint8_t *queue,
                                   uint8_t capacity,
                                   uint8_t *head,
                                   uint8_t *count,
                                   uint8_t *out_slot_index)
{
    if (*count == 0u) {
        return false;
    }

    *out_slot_index = queue[*head];
    *head = (uint8_t)((*head + 1u) % capacity);
    --(*count);
    return true;
}

static bool nb_event_bus_alloc_slot(nb_event_bus_t *bus,
                                    nb_event_priority_t priority,
                                    uint8_t *out_slot_index)
{
    if (priority == NB_EVENT_PRIORITY_SAFETY) {
        for (uint8_t i = 0; i < NB_EVENT_BUS_MAX_SLOTS; ++i) {
            if (!bus->slots[i].in_use && bus->slots[i].safety_reserved) {
                *out_slot_index = i;
                return true;
            }
        }
    }

    for (uint8_t i = 0; i < NB_EVENT_BUS_MAX_SLOTS; ++i) {
        if (!bus->slots[i].in_use && !bus->slots[i].safety_reserved) {
            *out_slot_index = i;
            return true;
        }
    }

    return false;
}

void nb_event_bus_init(nb_event_bus_t *bus)
{
    if (bus == NULL) {
        return;
    }

    memset(bus, 0, sizeof(*bus));

    for (uint8_t i = 0; i < NB_EVENT_BUS_MAX_SLOTS; ++i) {
        bus->slots[i].safety_reserved = i < NB_EVENT_BUS_SAFETY_RESERVED_SLOTS;
    }

    nb_event_bus_refresh_free_counts(bus);
}

nb_event_bus_status_t nb_event_bus_publish(nb_event_bus_t *bus,
                                           const nb_event_t *event)
{
    uint8_t slot_index = 0;
    bool queued = false;

    if (bus == NULL || event == NULL) {
        return NB_EVENT_BUS_ERR_INVALID_ARG;
    }
    if (event->payload_len > NB_EVENT_BUS_MAX_PAYLOAD_BYTES) {
        nb_event_bus_audit(bus, event, NB_EVENT_AUDIT_DROPPED,
                           NB_EVENT_BUS_ERR_PAYLOAD_TOO_LARGE);
        return NB_EVENT_BUS_ERR_PAYLOAD_TOO_LARGE;
    }
    if (event->priority != NB_EVENT_PRIORITY_NORMAL &&
        event->priority != NB_EVENT_PRIORITY_SAFETY) {
        nb_event_bus_audit(bus, event, NB_EVENT_AUDIT_DROPPED,
                           NB_EVENT_BUS_ERR_INVALID_ARG);
        return NB_EVENT_BUS_ERR_INVALID_ARG;
    }
    if (!nb_event_bus_alloc_slot(bus, event->priority, &slot_index)) {
        if (event->priority == NB_EVENT_PRIORITY_SAFETY) {
            ++bus->stats.dropped_safety;
        } else {
            ++bus->stats.dropped_normal;
        }
        nb_event_bus_refresh_free_counts(bus);
        nb_event_bus_audit(bus, event, NB_EVENT_AUDIT_DROPPED,
                           NB_EVENT_BUS_ERR_FULL);
        return NB_EVENT_BUS_ERR_FULL;
    }

    bus->slots[slot_index].event = *event;
    bus->slots[slot_index].in_use = true;

    if (event->priority == NB_EVENT_PRIORITY_SAFETY) {
        queued = nb_event_bus_queue_push(
            bus->safety_queue, NB_EVENT_BUS_MAX_SLOTS, &bus->safety_tail,
            &bus->safety_count, slot_index);
    } else {
        queued = nb_event_bus_queue_push(
            bus->normal_queue, NB_EVENT_BUS_NORMAL_SLOTS, &bus->normal_tail,
            &bus->normal_count, slot_index);
    }

    if (!queued) {
        bus->slots[slot_index].in_use = false;
        if (event->priority == NB_EVENT_PRIORITY_SAFETY) {
            ++bus->stats.dropped_safety;
        } else {
            ++bus->stats.dropped_normal;
        }
        nb_event_bus_refresh_free_counts(bus);
        nb_event_bus_audit(bus, event, NB_EVENT_AUDIT_DROPPED,
                           NB_EVENT_BUS_ERR_FULL);
        return NB_EVENT_BUS_ERR_FULL;
    }

    if (event->priority == NB_EVENT_PRIORITY_SAFETY) {
        ++bus->stats.published_safety;
    } else {
        ++bus->stats.published_normal;
    }

    nb_event_bus_refresh_free_counts(bus);
    nb_event_bus_audit(bus, event, NB_EVENT_AUDIT_PUBLISHED,
                       NB_EVENT_BUS_OK);
    return NB_EVENT_BUS_OK;
}

nb_event_bus_status_t nb_event_bus_poll(nb_event_bus_t *bus,
                                        nb_event_t *out_event)
{
    uint8_t slot_index = 0;
    bool found = false;

    if (bus == NULL || out_event == NULL) {
        return NB_EVENT_BUS_ERR_INVALID_ARG;
    }

    found = nb_event_bus_queue_pop(bus->safety_queue, NB_EVENT_BUS_MAX_SLOTS,
                                   &bus->safety_head, &bus->safety_count,
                                   &slot_index);
    if (!found) {
        found = nb_event_bus_queue_pop(bus->normal_queue,
                                       NB_EVENT_BUS_NORMAL_SLOTS,
                                       &bus->normal_head, &bus->normal_count,
                                       &slot_index);
    }
    if (!found) {
        return NB_EVENT_BUS_ERR_EMPTY;
    }

    *out_event = bus->slots[slot_index].event;
    bus->slots[slot_index].in_use = false;

    if (out_event->priority == NB_EVENT_PRIORITY_SAFETY) {
        ++bus->stats.polled_safety;
    } else {
        ++bus->stats.polled_normal;
    }

    nb_event_bus_refresh_free_counts(bus);
    nb_event_bus_audit(bus, out_event, NB_EVENT_AUDIT_POLLED,
                       NB_EVENT_BUS_OK);
    return NB_EVENT_BUS_OK;
}

void nb_event_bus_get_stats(const nb_event_bus_t *bus,
                            nb_event_bus_stats_t *out_stats)
{
    if (bus == NULL || out_stats == NULL) {
        return;
    }

    *out_stats = bus->stats;
}

size_t nb_event_bus_copy_audit(const nb_event_bus_t *bus,
                               nb_event_audit_entry_t *out_entries,
                               size_t max_entries)
{
    size_t copied = 0;
    uint8_t start = 0;

    if (bus == NULL || out_entries == NULL || max_entries == 0u) {
        return 0u;
    }

    if (bus->audit_count == NB_EVENT_BUS_AUDIT_CAPACITY) {
        start = bus->audit_head;
    }

    while (copied < bus->audit_count && copied < max_entries) {
        const uint8_t index =
            (uint8_t)((start + copied) % NB_EVENT_BUS_AUDIT_CAPACITY);
        out_entries[copied] = bus->audit[index];
        ++copied;
    }

    return copied;
}
