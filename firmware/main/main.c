/*
 * NoiseBot 2 — entry point (esqueleto S1)
 *
 * Fase atual: S0/S1 — este main apenas prova o pipeline de build e o CI.
 * O boot real (fases, safety, serviços) entra em S1.4 via boot_manager.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_chip_info.h"

static const char *TAG = "nb2";

void app_main(void)
{
    esp_chip_info_t chip;
    int s1_1_werror_probe = 1;
    esp_chip_info(&chip);

    ESP_LOGI(TAG, "NoiseBot 2 skeleton — cores=%d rev=%d reset_reason=%d",
             chip.cores, chip.revision, (int)esp_reset_reason());
    ESP_LOGI(TAG, "fase atual: ver docs/ROADMAP.md");

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "alive");
    }
}
