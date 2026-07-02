/*
 * NoiseBot 2 — entry point (esqueleto S1)
 *
 * Fase atual: S1.4 — boot real via boot_manager (logger + app_config).
 * S1.5: watchdog integrado ao app_main/TWDT.
 * event_bus ainda não entra na sequência: sua casca só nasce quando houver
 * serviço publicando evento (ver README do componente).
 */

#include "boot_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "nb_boot_manager_shell.h"
#include "nb_watchdog_shell.h"

#define NB_APP_MAIN_WATCHDOG_TIMEOUT_MS 10000u
#define NB_APP_MAIN_HEARTBEAT_MS 1000u

static const char *TAG = "nb2";

void app_main(void)
{
    esp_chip_info_t chip;
    nb_boot_report_t report;
    nb_boot_outcome_t outcome;

    esp_chip_info(&chip);

    ESP_LOGI(TAG, "NoiseBot 2 skeleton — cores=%d rev=%d reset_reason=%d",
             chip.cores, chip.revision, (int)esp_reset_reason());

    outcome = nb_boot_manager_shell_run(&report);
    ESP_LOGI(TAG, "boot: outcome=%d fases=%u duracao_total_ms=%u",
             (int)outcome, (unsigned)report.phase_count,
             (unsigned)report.total_duration_ms);

    ESP_ERROR_CHECK(nb_watchdog_shell_init(NB_APP_MAIN_WATCHDOG_TIMEOUT_MS));

    for (;;) {
        ESP_ERROR_CHECK(nb_watchdog_shell_feed());
        vTaskDelay(pdMS_TO_TICKS(NB_APP_MAIN_HEARTBEAT_MS));
        ESP_LOGI(TAG, "alive");
    }
}
