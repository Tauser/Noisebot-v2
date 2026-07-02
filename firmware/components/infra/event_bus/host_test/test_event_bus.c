#include "../event_bus.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);           \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static nb_event_t make_event(uint16_t type,
                             nb_event_priority_t priority,
                             uint32_t timestamp_ms)
{
    nb_event_t event;

    memset(&event, 0, sizeof(event));
    event.type = type;
    event.priority = priority;
    event.timestamp_ms = timestamp_ms;
    event.payload_len = 1u;
    event.payload[0] = (uint8_t)(type & 0xffu);
    return event;
}

static void test_normal_burst_has_zero_drop(void)
{
    nb_event_bus_t bus;
    nb_event_bus_stats_t stats;

    nb_event_bus_init(&bus);

    for (uint16_t i = 0; i < NB_EVENT_BUS_NORMAL_SLOTS; ++i) {
        nb_event_t event =
            make_event((uint16_t)(100u + i), NB_EVENT_PRIORITY_NORMAL, i);
        CHECK(nb_event_bus_publish(&bus, &event) == NB_EVENT_BUS_OK);
    }

    nb_event_bus_get_stats(&bus, &stats);
    CHECK(stats.published_normal == NB_EVENT_BUS_NORMAL_SLOTS);
    CHECK(stats.dropped_normal == 0u);
    CHECK(stats.normal_pending == NB_EVENT_BUS_NORMAL_SLOTS);
    CHECK(stats.free_safety_slots == NB_EVENT_BUS_SAFETY_RESERVED_SLOTS);

    for (uint16_t i = 0; i < NB_EVENT_BUS_NORMAL_SLOTS; ++i) {
        nb_event_t out;
        CHECK(nb_event_bus_poll(&bus, &out) == NB_EVENT_BUS_OK);
        CHECK(out.priority == NB_EVENT_PRIORITY_NORMAL);
        CHECK(out.type == (uint16_t)(100u + i));
    }

    CHECK(nb_event_bus_poll(&bus, &(nb_event_t){0}) ==
          NB_EVENT_BUS_ERR_EMPTY);
}

static void test_safety_is_immune_to_full_normal_queue(void)
{
    nb_event_bus_t bus;
    nb_event_bus_stats_t stats;
    nb_event_t safety = make_event(7u, NB_EVENT_PRIORITY_SAFETY, 900u);
    nb_event_t extra_normal = make_event(99u, NB_EVENT_PRIORITY_NORMAL, 901u);
    nb_event_t out;

    nb_event_bus_init(&bus);

    for (uint16_t i = 0; i < NB_EVENT_BUS_NORMAL_SLOTS; ++i) {
        nb_event_t event =
            make_event((uint16_t)(200u + i), NB_EVENT_PRIORITY_NORMAL, i);
        CHECK(nb_event_bus_publish(&bus, &event) == NB_EVENT_BUS_OK);
    }

    CHECK(nb_event_bus_publish(&bus, &extra_normal) == NB_EVENT_BUS_ERR_FULL);
    CHECK(nb_event_bus_publish(&bus, &safety) == NB_EVENT_BUS_OK);

    nb_event_bus_get_stats(&bus, &stats);
    CHECK(stats.dropped_normal == 1u);
    CHECK(stats.dropped_safety == 0u);
    CHECK(stats.safety_pending == 1u);

    CHECK(nb_event_bus_poll(&bus, &out) == NB_EVENT_BUS_OK);
    CHECK(out.priority == NB_EVENT_PRIORITY_SAFETY);
    CHECK(out.type == 7u);
}

static void test_audit_ring_records_drops_and_polls(void)
{
    nb_event_bus_t bus;
    nb_event_audit_entry_t audit[NB_EVENT_BUS_AUDIT_CAPACITY];
    nb_event_t out;
    size_t count;

    nb_event_bus_init(&bus);

    for (uint16_t i = 0; i < NB_EVENT_BUS_NORMAL_SLOTS; ++i) {
        nb_event_t event =
            make_event((uint16_t)(300u + i), NB_EVENT_PRIORITY_NORMAL, i);
        CHECK(nb_event_bus_publish(&bus, &event) == NB_EVENT_BUS_OK);
    }

    CHECK(nb_event_bus_publish(
              &bus, &(nb_event_t){.type = 999u,
                                   .priority = NB_EVENT_PRIORITY_NORMAL,
                                   .payload_len = 0u}) ==
          NB_EVENT_BUS_ERR_FULL);
    CHECK(nb_event_bus_poll(&bus, &out) == NB_EVENT_BUS_OK);

    count = nb_event_bus_copy_audit(&bus, audit, NB_EVENT_BUS_AUDIT_CAPACITY);
    CHECK(count == (size_t)NB_EVENT_BUS_NORMAL_SLOTS + 2u);
    CHECK(audit[count - 2u].action == NB_EVENT_AUDIT_DROPPED);
    CHECK(audit[count - 2u].status == NB_EVENT_BUS_ERR_FULL);
    CHECK(audit[count - 1u].action == NB_EVENT_AUDIT_POLLED);
}

static void test_invalid_payload_is_rejected(void)
{
    nb_event_bus_t bus;
    nb_event_t event = make_event(1u, NB_EVENT_PRIORITY_NORMAL, 1u);

    nb_event_bus_init(&bus);
    event.payload_len = (uint8_t)(NB_EVENT_BUS_MAX_PAYLOAD_BYTES + 1u);

    CHECK(nb_event_bus_publish(&bus, &event) ==
          NB_EVENT_BUS_ERR_PAYLOAD_TOO_LARGE);
}

int main(void)
{
    test_normal_burst_has_zero_drop();
    test_safety_is_immune_to_full_normal_queue();
    test_audit_ring_records_drops_and_polls();
    test_invalid_payload_is_rejected();

    if (failures != 0) {
        printf("event_bus host_test: %d failure(s)\n", failures);
        return 1;
    }

    printf("event_bus host_test: ok\n");
    return 0;
}
