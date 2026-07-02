#include "watchdog.h"

#include <assert.h>
#include <string.h>

static void test_register_and_feed(void)
{
    nb_watchdog_t watchdog;
    nb_watchdog_check_result_t result;
    uint8_t task_id;

    nb_watchdog_init(&watchdog);

    assert(nb_watchdog_register_task(&watchdog, "main", 10u, 100u, &task_id) ==
           NB_WATCHDOG_STATUS_OK);
    assert(task_id == 0u);
    assert(!nb_watchdog_check(&watchdog, 100u, &result));

    assert(nb_watchdog_feed(&watchdog, task_id, 150u) == NB_WATCHDOG_STATUS_OK);
    assert(!nb_watchdog_check(&watchdog, 240u, &result));
}

static void test_timeout_reports_first_stale_task(void)
{
    nb_watchdog_t watchdog;
    nb_watchdog_check_result_t result;
    uint8_t first_id;
    uint8_t second_id;

    nb_watchdog_init(&watchdog);

    assert(nb_watchdog_register_task(&watchdog, "render", 0u, 50u, &first_id) ==
           NB_WATCHDOG_STATUS_OK);
    assert(nb_watchdog_register_task(&watchdog, "storage", 40u, 200u, &second_id) ==
           NB_WATCHDOG_STATUS_OK);

    assert(nb_watchdog_check(&watchdog, 60u, &result));
    assert(result.expired);
    assert(result.task_id == first_id);
    assert(strcmp(result.task_name, "render") == 0);
    assert(result.elapsed_ms == 60u);
}

static void test_duplicate_and_capacity_are_rejected(void)
{
    nb_watchdog_t watchdog;
    uint8_t task_id;
    uint8_t i;
    char name[] = "task_00";

    nb_watchdog_init(&watchdog);

    assert(nb_watchdog_register_task(&watchdog, "main", 0u, 100u, &task_id) ==
           NB_WATCHDOG_STATUS_OK);
    assert(nb_watchdog_register_task(&watchdog, "main", 0u, 100u, &task_id) ==
           NB_WATCHDOG_STATUS_DUPLICATE);

    for (i = 1u; i < NB_WATCHDOG_MAX_TASKS; ++i) {
        name[5] = (char)('0' + (i / 10u));
        name[6] = (char)('0' + (i % 10u));
        assert(nb_watchdog_register_task(&watchdog, name, 0u, 100u, &task_id) ==
               NB_WATCHDOG_STATUS_OK);
    }

    assert(nb_watchdog_register_task(&watchdog, "overflow", 0u, 100u, &task_id) ==
           NB_WATCHDOG_STATUS_FULL);
}

static void test_invalid_arguments_do_not_mutate(void)
{
    nb_watchdog_t watchdog;
    uint8_t task_id = 99u;

    nb_watchdog_init(&watchdog);

    assert(nb_watchdog_register_task(&watchdog, NULL, 0u, 100u, &task_id) ==
           NB_WATCHDOG_STATUS_INVALID_ARG);
    assert(nb_watchdog_register_task(&watchdog, "", 0u, 100u, &task_id) ==
           NB_WATCHDOG_STATUS_INVALID_ARG);
    assert(nb_watchdog_register_task(&watchdog, "main", 0u, 0u, &task_id) ==
           NB_WATCHDOG_STATUS_INVALID_ARG);
    assert(watchdog.task_count == 0u);
    assert(nb_watchdog_feed(&watchdog, 0u, 10u) == NB_WATCHDOG_STATUS_INVALID_ARG);
}

static void test_reset_cause_is_stored_in_core(void)
{
    nb_watchdog_t watchdog;

    nb_watchdog_init(&watchdog);
    assert(nb_watchdog_get_last_reset_cause(&watchdog) == NB_WATCHDOG_RESET_CAUSE_NONE);

    nb_watchdog_set_last_reset_cause(&watchdog, NB_WATCHDOG_RESET_CAUSE_TASK_TIMEOUT);
    assert(nb_watchdog_get_last_reset_cause(&watchdog) ==
           NB_WATCHDOG_RESET_CAUSE_TASK_TIMEOUT);
}

int main(void)
{
    test_register_and_feed();
    test_timeout_reports_first_stale_task();
    test_duplicate_and_capacity_are_rejected();
    test_invalid_arguments_do_not_mutate();
    test_reset_cause_is_stored_in_core();
    return 0;
}
