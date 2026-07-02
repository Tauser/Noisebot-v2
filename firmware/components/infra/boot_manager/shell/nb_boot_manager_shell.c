#include "nb_boot_manager_shell.h"

#include "app_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nb_app_config_shell.h"
#include "nb_logger_shell.h"

static const char *TAG = "boot_manager";

static uint32_t nb_boot_manager_shell_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

nb_boot_outcome_t nb_boot_manager_shell_run(nb_boot_report_t *out_report)
{
    nb_boot_manager_t manager;
    nb_boot_report_t report;
    uint32_t fail_streak;

    nb_boot_manager_init(&manager);

    nb_boot_manager_begin_phase(&manager, "logger", NB_BOOT_PHASE_CRITICAL,
                                nb_boot_manager_shell_now_ms());
    nb_boot_manager_end_phase(&manager, nb_logger_shell_init(NB_LOG_LEVEL_INFO) == ESP_OK,
                              nb_boot_manager_shell_now_ms());

    nb_boot_manager_begin_phase(&manager, "app_config", NB_BOOT_PHASE_CRITICAL,
                                nb_boot_manager_shell_now_ms());
    nb_boot_manager_end_phase(&manager, nb_app_config_shell_init() == ESP_OK,
                              nb_boot_manager_shell_now_ms());

    nb_boot_manager_get_report(&manager, &report);

    if (report.outcome == NB_BOOT_OUTCOME_SAFE_MODE) {
        ESP_LOGE(TAG, "SAFE_MODE: fase '%s' falhou", report.safe_mode_reason);
        if (nb_app_config_shell_get_u32(NB_CONFIG_KEY_BOOT_FAIL_STREAK, &fail_streak)) {
            nb_app_config_shell_set_u32(NB_CONFIG_KEY_BOOT_FAIL_STREAK, fail_streak + 1u);
        }
    } else {
        nb_app_config_shell_set_u32(NB_CONFIG_KEY_BOOT_FAIL_STREAK, 0u);
    }

    if (out_report != NULL) {
        *out_report = report;
    }

    return report.outcome;
}
