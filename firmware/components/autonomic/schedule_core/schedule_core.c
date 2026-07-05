#include "schedule_core.h"

#include <string.h>

void nb_schedule_core_init(nb_schedule_core_t *core) {
    if (!core) {
        return;
    }
    memset(core, 0, sizeof(*core));
}

static nb_schedule_timer_t *find_slot(nb_schedule_core_t *core, uint32_t timer_id) {
    for (uint32_t i = 0; i < NB_SCHEDULE_MAX_TIMERS; ++i) {
        if (core->slots[i].in_use && core->slots[i].timer_id == timer_id) {
            return &core->slots[i];
        }
    }
    return NULL;
}

static nb_schedule_timer_t *find_free_slot(nb_schedule_core_t *core) {
    for (uint32_t i = 0; i < NB_SCHEDULE_MAX_TIMERS; ++i) {
        if (!core->slots[i].in_use) {
            return &core->slots[i];
        }
    }
    return NULL;
}

nb_schedule_status_t nb_schedule_core_create(nb_schedule_core_t *core, uint32_t timer_id,
                                             uint64_t fire_at_unix_ms, const char *label) {
    if (!core) {
        return NB_SCHEDULE_STATUS_ERR_INVALID_ARG;
    }

    nb_schedule_timer_t *slot = find_slot(core, timer_id);
    if (!slot) {
        slot = find_free_slot(core);
        if (!slot) {
            return NB_SCHEDULE_STATUS_ERR_FULL;
        }
    }

    slot->in_use = true;
    slot->timer_id = timer_id;
    slot->fire_at_unix_ms = fire_at_unix_ms;
    memset(slot->label, 0, sizeof(slot->label));
    if (label) {
        strncpy(slot->label, label, NB_SCHEDULE_LABEL_MAX_LEN);
    }

    return NB_SCHEDULE_STATUS_OK;
}

nb_schedule_status_t nb_schedule_core_cancel(nb_schedule_core_t *core, uint32_t timer_id) {
    if (!core) {
        return NB_SCHEDULE_STATUS_ERR_INVALID_ARG;
    }

    nb_schedule_timer_t *slot = find_slot(core, timer_id);
    if (!slot) {
        return NB_SCHEDULE_STATUS_ERR_NOT_FOUND;
    }

    memset(slot, 0, sizeof(*slot));
    return NB_SCHEDULE_STATUS_OK;
}

uint32_t nb_schedule_core_tick(nb_schedule_core_t *core, uint64_t now_unix_ms,
                               uint32_t *out_fired_ids, uint32_t max_fired) {
    if (!core || !out_fired_ids) {
        return 0u;
    }

    uint32_t fired_count = 0u;
    for (uint32_t i = 0; i < NB_SCHEDULE_MAX_TIMERS && fired_count < max_fired; ++i) {
        nb_schedule_timer_t *slot = &core->slots[i];
        if (slot->in_use && slot->fire_at_unix_ms <= now_unix_ms) {
            out_fired_ids[fired_count] = slot->timer_id;
            ++fired_count;
            memset(slot, 0, sizeof(*slot));
        }
    }
    return fired_count;
}

bool nb_schedule_core_get(const nb_schedule_core_t *core, uint32_t timer_id,
                          nb_schedule_timer_t *out_timer) {
    if (!core || !out_timer) {
        return false;
    }
    for (uint32_t i = 0; i < NB_SCHEDULE_MAX_TIMERS; ++i) {
        if (core->slots[i].in_use && core->slots[i].timer_id == timer_id) {
            *out_timer = core->slots[i];
            return true;
        }
    }
    return false;
}

uint32_t nb_schedule_core_count(const nb_schedule_core_t *core) {
    if (!core) {
        return 0u;
    }
    uint32_t count = 0u;
    for (uint32_t i = 0; i < NB_SCHEDULE_MAX_TIMERS; ++i) {
        if (core->slots[i].in_use) {
            ++count;
        }
    }
    return count;
}
