/*
 * NoiseBot 2 — entry point (esqueleto S1)
 *
 * Fase atual: S1.4 — boot real via boot_manager (logger + app_config).
 * S1.5: watchdog integrado ao app_main/TWDT.
 * S1.6: wifi_setup (SoftAP provisioning) — falha aqui não trava o robô,
 * que continua funcional offline (P1: corpo/mente separados).
 * S1.7: mind_link (sessão NBP/2 sobre TCP) — mesma regra, reconecta com
 * backoff e nunca bloqueia o boot se o server estiver offline.
 * S1.8: confirmação local de imagem OTA pendente só depois do esqueleto subir
 * saudável; Secure Boot/flash encryption exigem gate de bancada explícito.
 * S2.1: display_hal — driver ST7789 + double buffer PSRAM.
 * S2.2: renderer paramétrico (face_renderer) — cicla as 10 expressões-base
 * com interpolação de 220 ms, numa task própria, para comparação visual
 * lado a lado com o v1 em bancada.
 * event_bus ainda não entra na sequência: sua casca só nasce quando houver
 * serviço publicando evento (ver README do componente).
 */

#include <stdbool.h>

#include "boot_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "nb_boot_manager_shell.h"
#include "nb_watchdog_shell.h"
#include "nb_wifi_setup_shell.h"
#include "nb_ota_shell.h"
#include "nb_mind_link_shell.h"
#include "nb_display_hal_shell.h"
#include "nb_face_renderer_shell.h"

#define NB_APP_MAIN_WATCHDOG_TIMEOUT_MS 10000u
#define NB_APP_MAIN_HEARTBEAT_MS 1000u
#define NB_APP_MAIN_FACE_DEMO_STACK 4096u
#define NB_APP_MAIN_FACE_DEMO_PRIO 8
#define NB_APP_MAIN_FACE_DEMO_TICK_MS 33u
#define NB_APP_MAIN_FACE_DEMO_HOLD_MS 1500u
#define NB_APP_MAIN_FACE_DEMO_TRANSITION_MS 220u

static const char *TAG = "nb2";

/* Cicla as NB_FACE_EXPR_COUNT expressões-base com interpolação de 220 ms
 * (VISUAL.md §1/§2) — padrão de bring-up do S2.2 para comparação visual
 * lado a lado com o v1; substitui o padrão de barras do S2.1. */
static void nb_app_main_face_demo_task(void *arg)
{
    uint32_t from_expr = 0;
    uint32_t to_expr = 1;
    uint32_t elapsed_ms = 0;
    bool transitioning = true;

    (void)arg;

    for (;;) {
        const nb_face_state_t *from = nb_face_core_get_expression((nb_face_expr_t)from_expr);
        const nb_face_state_t *to = nb_face_core_get_expression((nb_face_expr_t)to_expr);
        nb_face_state_t current;

        if (transitioning) {
            const float t = (float)elapsed_ms / (float)NB_APP_MAIN_FACE_DEMO_TRANSITION_MS;
            nb_face_core_lerp(from, to, (t > 1.0f) ? 1.0f : t, &current);
        } else {
            current = *to;
        }

        nb_face_renderer_shell_draw(&current, 0.0f, 0.0f, 0xffffffU);

        esp_err_t err = nb_display_hal_shell_flush_and_swap();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "display flush falhou (%s)", esp_err_to_name(err));
        }
        err = nb_face_renderer_shell_bind_buffer(nb_display_hal_shell_get_back_buffer());
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "bind do back buffer falhou (%s)", esp_err_to_name(err));
        }

        elapsed_ms += NB_APP_MAIN_FACE_DEMO_TICK_MS;
        if (transitioning && elapsed_ms >= NB_APP_MAIN_FACE_DEMO_TRANSITION_MS) {
            transitioning = false;
            elapsed_ms = 0;
        } else if (!transitioning && elapsed_ms >= NB_APP_MAIN_FACE_DEMO_HOLD_MS) {
            from_expr = to_expr;
            to_expr = (to_expr + 1u) % NB_FACE_EXPR_COUNT;
            transitioning = true;
            elapsed_ms = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(NB_APP_MAIN_FACE_DEMO_TICK_MS));
    }
}

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

    esp_err_t wifi_err = nb_wifi_setup_shell_init();
    if (wifi_err != ESP_OK) {
        ESP_LOGE(TAG, "wifi_setup falhou (%s) — seguindo offline",
                 esp_err_to_name(wifi_err));
    }

    esp_err_t mind_link_err = nb_mind_link_shell_init();
    if (mind_link_err != ESP_OK) {
        ESP_LOGE(TAG, "mind_link falhou (%s) — seguindo offline",
                 esp_err_to_name(mind_link_err));
    }

    if (outcome == NB_BOOT_OUTCOME_OK) {
        esp_err_t ota_err = nb_ota_shell_confirm_boot_if_pending();
        if (ota_err != ESP_OK) {
            ESP_LOGE(TAG, "confirmacao OTA falhou (%s)",
                     esp_err_to_name(ota_err));
        }
    } else {
        ESP_LOGW(TAG, "boot nao saudavel; imagem OTA pendente nao sera confirmada");
    }

    esp_err_t display_err = nb_display_hal_shell_init();
    if (display_err != ESP_OK) {
        ESP_LOGE(TAG, "display_hal falhou (%s)", esp_err_to_name(display_err));
    } else {
        esp_err_t bind_err =
            nb_face_renderer_shell_bind_buffer(nb_display_hal_shell_get_back_buffer());
        if (bind_err != ESP_OK) {
            ESP_LOGE(TAG, "bind inicial do face_renderer falhou (%s)",
                     esp_err_to_name(bind_err));
        } else {
            xTaskCreate(nb_app_main_face_demo_task, "face_demo", NB_APP_MAIN_FACE_DEMO_STACK,
                       NULL, NB_APP_MAIN_FACE_DEMO_PRIO, NULL);
        }
    }

    for (;;) {
        ESP_ERROR_CHECK(nb_watchdog_shell_feed());
        vTaskDelay(pdMS_TO_TICKS(NB_APP_MAIN_HEARTBEAT_MS));
        ESP_LOGI(TAG, "alive");
    }
}
