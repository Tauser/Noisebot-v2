#include "watchdog.h"

#include <stddef.h>
#include <string.h>

static bool nb_watchdog_name_equals(const char *left, const char *right)
{
    return strncmp(left, right, NB_WATCHDOG_TASK_NAME_MAX) == 0;
}

static void nb_watchdog_copy_name(char *destination, const char *source)
{
    size_t i;

    for (i = 0; i + 1u < NB_WATCHDOG_TASK_NAME_MAX && source[i] != '\0'; ++i) {
        destination[i] = source[i];
    }
    destination[i] = '\0';
}

void nb_watchdog_init(nb_watchdog_t *watchdog)
{
    if (watchdog == NULL) {
        return;
    }

    memset(watchdog, 0, sizeof(*watchdog));
    watchdog->last_reset_cause = NB_WATCHDOG_RESET_CAUSE_NONE;
}

nb_watchdog_status_t nb_watchdog_register_task(nb_watchdog_t *watchdog,
                                               const char *name,
                                               uint32_t now_ms,
                                               uint32_t timeout_ms,
                                               uint8_t *out_task_id)
{
    uint8_t id;

    if (watchdog == NULL || name == NULL || name[0] == '\0' || timeout_ms == 0u ||
        out_task_id == NULL) {
        return NB_WATCHDOG_STATUS_INVALID_ARG;
    }

    for (id = 0u; id < watchdog->task_count; ++id) {
        if (watchdog->tasks[id].active && nb_watchdog_name_equals(watchdog->tasks[id].name, name)) {
            return NB_WATCHDOG_STATUS_DUPLICATE;
        }
    }

    if (watchdog->task_count >= NB_WATCHDOG_MAX_TASKS) {
        return NB_WATCHDOG_STATUS_FULL;
    }

    id = watchdog->task_count;
    nb_watchdog_copy_name(watchdog->tasks[id].name, name);
    watchdog->tasks[id].last_feed_ms = now_ms;
    watchdog->tasks[id].timeout_ms = timeout_ms;
    watchdog->tasks[id].active = true;
    watchdog->task_count++;
    *out_task_id = id;

    return NB_WATCHDOG_STATUS_OK;
}

nb_watchdog_status_t nb_watchdog_feed(nb_watchdog_t *watchdog,
                                      uint8_t task_id,
                                      uint32_t now_ms)
{
    if (watchdog == NULL || task_id >= watchdog->task_count || !watchdog->tasks[task_id].active) {
        return NB_WATCHDOG_STATUS_INVALID_ARG;
    }

    watchdog->tasks[task_id].last_feed_ms = now_ms;
    return NB_WATCHDOG_STATUS_OK;
}

bool nb_watchdog_check(const nb_watchdog_t *watchdog,
                       uint32_t now_ms,
                       nb_watchdog_check_result_t *out_result)
{
    uint8_t id;

    if (out_result != NULL) {
        memset(out_result, 0, sizeof(*out_result));
    }
    if (watchdog == NULL) {
        return false;
    }

    for (id = 0u; id < watchdog->task_count; ++id) {
        const nb_watchdog_task_t *task = &watchdog->tasks[id];
        uint32_t elapsed_ms;

        if (!task->active) {
            continue;
        }

        elapsed_ms = now_ms - task->last_feed_ms;
        if (elapsed_ms > task->timeout_ms) {
            if (out_result != NULL) {
                out_result->expired = true;
                out_result->task_id = id;
                nb_watchdog_copy_name(out_result->task_name, task->name);
                out_result->elapsed_ms = elapsed_ms;
            }
            return true;
        }
    }

    return false;
}

void nb_watchdog_set_last_reset_cause(nb_watchdog_t *watchdog,
                                      nb_watchdog_reset_cause_t cause)
{
    if (watchdog == NULL) {
        return;
    }

    watchdog->last_reset_cause = cause;
}

nb_watchdog_reset_cause_t nb_watchdog_get_last_reset_cause(const nb_watchdog_t *watchdog)
{
    if (watchdog == NULL) {
        return NB_WATCHDOG_RESET_CAUSE_NONE;
    }

    return watchdog->last_reset_cause;
}
