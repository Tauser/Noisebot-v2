#ifndef NB_WATCHDOG_H
#define NB_WATCHDOG_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NB_WATCHDOG_MAX_TASKS 16u
#define NB_WATCHDOG_TASK_NAME_MAX 24u

typedef enum {
    NB_WATCHDOG_STATUS_OK = 0,
    NB_WATCHDOG_STATUS_FULL,
    NB_WATCHDOG_STATUS_INVALID_ARG,
    NB_WATCHDOG_STATUS_DUPLICATE,
} nb_watchdog_status_t;

typedef enum {
    NB_WATCHDOG_RESET_CAUSE_NONE = 0,
    NB_WATCHDOG_RESET_CAUSE_TASK_TIMEOUT,
    NB_WATCHDOG_RESET_CAUSE_HW_WDT,
    NB_WATCHDOG_RESET_CAUSE_UNKNOWN_WDT,
} nb_watchdog_reset_cause_t;

typedef struct {
    char name[NB_WATCHDOG_TASK_NAME_MAX];
    uint32_t last_feed_ms;
    uint32_t timeout_ms;
    bool active;
} nb_watchdog_task_t;

typedef struct {
    bool expired;
    uint8_t task_id;
    char task_name[NB_WATCHDOG_TASK_NAME_MAX];
    uint32_t elapsed_ms;
} nb_watchdog_check_result_t;

typedef struct {
    nb_watchdog_task_t tasks[NB_WATCHDOG_MAX_TASKS];
    uint8_t task_count;
    nb_watchdog_reset_cause_t last_reset_cause;
} nb_watchdog_t;

void nb_watchdog_init(nb_watchdog_t *watchdog);

nb_watchdog_status_t nb_watchdog_register_task(nb_watchdog_t *watchdog,
                                               const char *name,
                                               uint32_t now_ms,
                                               uint32_t timeout_ms,
                                               uint8_t *out_task_id);

nb_watchdog_status_t nb_watchdog_feed(nb_watchdog_t *watchdog,
                                      uint8_t task_id,
                                      uint32_t now_ms);

bool nb_watchdog_check(const nb_watchdog_t *watchdog,
                       uint32_t now_ms,
                       nb_watchdog_check_result_t *out_result);

void nb_watchdog_set_last_reset_cause(nb_watchdog_t *watchdog,
                                      nb_watchdog_reset_cause_t cause);

nb_watchdog_reset_cause_t nb_watchdog_get_last_reset_cause(const nb_watchdog_t *watchdog);

#ifdef __cplusplus
}
#endif

#endif
