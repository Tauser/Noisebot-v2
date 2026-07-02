#include "../boot_manager.h"

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

static void test_all_phases_ok_sums_duration_and_reports_ok(void)
{
    nb_boot_manager_t manager;
    nb_boot_report_t report;

    nb_boot_manager_init(&manager);

    CHECK(nb_boot_manager_begin_phase(&manager, "logger", NB_BOOT_PHASE_CRITICAL, 0u));
    CHECK(nb_boot_manager_end_phase(&manager, true, 10u));
    CHECK(nb_boot_manager_begin_phase(&manager, "wifi", NB_BOOT_PHASE_NON_CRITICAL, 10u));
    CHECK(nb_boot_manager_end_phase(&manager, true, 35u));

    nb_boot_manager_get_report(&manager, &report);
    CHECK(report.phase_count == 2u);
    CHECK(report.total_duration_ms == 35u);
    CHECK(report.outcome == NB_BOOT_OUTCOME_OK);
    CHECK(strcmp(report.phases[0].name, "logger") == 0 && report.phases[0].duration_ms == 10u);
    CHECK(strcmp(report.phases[1].name, "wifi") == 0 && report.phases[1].duration_ms == 25u);
}

static void test_non_critical_failure_does_not_trigger_safe_mode(void)
{
    nb_boot_manager_t manager;
    nb_boot_report_t report;

    nb_boot_manager_init(&manager);

    CHECK(nb_boot_manager_begin_phase(&manager, "camera", NB_BOOT_PHASE_NON_CRITICAL, 0u));
    CHECK(nb_boot_manager_end_phase(&manager, false, 5u));

    nb_boot_manager_get_report(&manager, &report);
    CHECK(report.outcome == NB_BOOT_OUTCOME_OK);
}

static void test_critical_failure_triggers_safe_mode_with_reason(void)
{
    nb_boot_manager_t manager;
    nb_boot_report_t report;

    nb_boot_manager_init(&manager);

    CHECK(nb_boot_manager_begin_phase(&manager, "sdmmc", NB_BOOT_PHASE_CRITICAL, 0u));
    CHECK(nb_boot_manager_end_phase(&manager, false, 3u));
    CHECK(nb_boot_manager_begin_phase(&manager, "wifi", NB_BOOT_PHASE_NON_CRITICAL, 3u));
    CHECK(nb_boot_manager_end_phase(&manager, true, 8u));

    nb_boot_manager_get_report(&manager, &report);
    CHECK(report.outcome == NB_BOOT_OUTCOME_SAFE_MODE);
    CHECK(strcmp(report.safe_mode_reason, "sdmmc") == 0);
}

static void test_safe_mode_reason_is_sticky_to_first_critical_failure(void)
{
    nb_boot_manager_t manager;
    nb_boot_report_t report;

    nb_boot_manager_init(&manager);

    CHECK(nb_boot_manager_begin_phase(&manager, "sdmmc", NB_BOOT_PHASE_CRITICAL, 0u));
    CHECK(nb_boot_manager_end_phase(&manager, false, 3u));
    CHECK(nb_boot_manager_begin_phase(&manager, "watchdog", NB_BOOT_PHASE_CRITICAL, 3u));
    CHECK(nb_boot_manager_end_phase(&manager, false, 6u));

    nb_boot_manager_get_report(&manager, &report);
    CHECK(report.outcome == NB_BOOT_OUTCOME_SAFE_MODE);
    CHECK(strcmp(report.safe_mode_reason, "sdmmc") == 0);
}

static void test_end_without_begin_is_rejected(void)
{
    nb_boot_manager_t manager;

    nb_boot_manager_init(&manager);

    CHECK(!nb_boot_manager_end_phase(&manager, true, 1u));
}

static void test_begin_without_ending_previous_is_rejected(void)
{
    nb_boot_manager_t manager;

    nb_boot_manager_init(&manager);

    CHECK(nb_boot_manager_begin_phase(&manager, "logger", NB_BOOT_PHASE_CRITICAL, 0u));
    CHECK(!nb_boot_manager_begin_phase(&manager, "config", NB_BOOT_PHASE_CRITICAL, 1u));
}

static void test_capacity_overflow_is_rejected(void)
{
    nb_boot_manager_t manager;
    nb_boot_report_t report;
    char name[8];

    nb_boot_manager_init(&manager);

    for (uint32_t i = 0; i < NB_BOOT_MANAGER_MAX_PHASES; ++i) {
        snprintf(name, sizeof(name), "p%u", (unsigned)i);
        CHECK(nb_boot_manager_begin_phase(&manager, name, NB_BOOT_PHASE_NON_CRITICAL, i));
        CHECK(nb_boot_manager_end_phase(&manager, true, i + 1u));
    }

    CHECK(!nb_boot_manager_begin_phase(&manager, "overflow", NB_BOOT_PHASE_NON_CRITICAL, 100u));

    nb_boot_manager_get_report(&manager, &report);
    CHECK(report.phase_count == NB_BOOT_MANAGER_MAX_PHASES);
}

int main(void)
{
    test_all_phases_ok_sums_duration_and_reports_ok();
    test_non_critical_failure_does_not_trigger_safe_mode();
    test_critical_failure_triggers_safe_mode_with_reason();
    test_safe_mode_reason_is_sticky_to_first_critical_failure();
    test_end_without_begin_is_rejected();
    test_begin_without_ending_previous_is_rejected();
    test_capacity_overflow_is_rejected();

    if (failures == 0) {
        printf("boot_manager host_test: ok\n");
        return 0;
    }

    printf("boot_manager host_test: %d failure(s)\n", failures);
    return 1;
}
