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
 * com interpolação de 220 ms; gate fechado (paridade visual + fps >= 30
 * confirmados em bancada).
 * S2.4: idle_engine — todos os motifs de VISUAL.md §3 (drift, blink,
 * peeks/scans, largura por olho de CURIOUS_TILT, roll de
 * HEAD_TILT_HOLD) sobre a expressão corrente, numa task própria.
 * S2.5: emotion_core v0 — pulso de estímulo sintético a cada ~90s
 * (substituto do touch real, que só chega em S3.1), decaindo pra
 * NEUTRAL; a expressão-âncora mais próxima do vetor troca com
 * interpolação de 220 ms, igual ao S2.2.
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
#include "esp_random.h"
#include "nb_boot_manager_shell.h"
#include "nb_watchdog_shell.h"
#include "nb_wifi_setup_shell.h"
#include "nb_ota_shell.h"
#include "nb_mind_link_shell.h"
#include "nb_display_hal_shell.h"
#include "nb_face_renderer_shell.h"
#include "idle_engine.h"
#include "emotion_core.h"

#define NB_APP_MAIN_WATCHDOG_TIMEOUT_MS 10000u
#define NB_APP_MAIN_HEARTBEAT_MS 1000u
#define NB_APP_MAIN_FACE_DEMO_STACK 4096u
#define NB_APP_MAIN_FACE_DEMO_PRIO 8
#define NB_APP_MAIN_FACE_DEMO_TICK_MS 33u
#define NB_APP_MAIN_FACE_DEMO_TRANSITION_MS 220u
/* Precisa ser bem maior que a constante de decaimento (~60s até <5% do
 * pico, BEHAVIOR.md §2) -- bug real achado em bancada: com 15s o pulso
 * seguinte chegava antes do vetor decair, acumulando (0.6*e^-15/tau ainda
 * é maioria do pulso) até travar perto do teto, oscilando entre
 * HAPPY/CURIOUS sem nunca voltar pra NEUTRAL. 90s dá folga real pro
 * decaimento completar entre pulsos. */
#define NB_APP_MAIN_EMOTION_STIMULUS_PERIOD_MS 90000u

static const char *TAG = "nb2";

/* idle_engine (S2.4) sobrepõe motifs de VISUAL.md §3 à expressão-âncora
 * mais próxima do vetor do emotion_core (S2.5). Um pulso de estímulo
 * sintético a cada ~90s substitui o toque real (S3.1, ainda não existe),
 * pra tornar visível em bancada a subida + decaimento pra NEUTRAL —
 * critério de VISUAL.md §2/BEHAVIOR.md §2. */
static void nb_app_main_face_demo_task(void *arg)
{
    nb_idle_engine_t idle;
    nb_emotion_state_t emotion;
    nb_face_expr_t from_expr = NB_FACE_EXPR_NEUTRAL;
    nb_face_expr_t to_expr = NB_FACE_EXPR_NEUTRAL;
    uint32_t transition_elapsed_ms = 0;
    uint32_t since_stimulus_ms = 0;
    bool transitioning = false;

    (void)arg;

    nb_idle_engine_init(&idle, esp_random());
    nb_emotion_core_init(&emotion);

    for (;;) {
        nb_idle_output_t idle_out;

        nb_emotion_core_tick(&emotion, NB_APP_MAIN_FACE_DEMO_TICK_MS);
        nb_idle_engine_tick(&idle, NB_APP_MAIN_FACE_DEMO_TICK_MS, &idle_out);

        since_stimulus_ms += NB_APP_MAIN_FACE_DEMO_TICK_MS;
        if (since_stimulus_ms >= NB_APP_MAIN_EMOTION_STIMULUS_PERIOD_MS) {
            since_stimulus_ms = 0;
            /* Pulso positivo (substitui TOUCH_TAP real, BEHAVIOR.md §2). */
            nb_emotion_core_apply_stimulus(&emotion, 0.6f, 0.4f);
        }

        const nb_face_expr_t nearest = nb_emotion_core_nearest_expression(&emotion);
        if (nearest != to_expr) {
            /* to_expr é sempre onde a face está agora (parada) ou pra onde
             * está indo (em transição) -- vale como ponto de partida da
             * nova transição nos dois casos. */
            from_expr = to_expr;
            to_expr = nearest;
            transitioning = true;
            transition_elapsed_ms = 0;
        }

        nb_face_state_t current;
        if (transitioning) {
            const float t =
                (float)transition_elapsed_ms / (float)NB_APP_MAIN_FACE_DEMO_TRANSITION_MS;
            nb_face_core_lerp(nb_face_core_get_expression(from_expr),
                              nb_face_core_get_expression(to_expr), (t > 1.0f) ? 1.0f : t,
                              &current);
            transition_elapsed_ms += NB_APP_MAIN_FACE_DEMO_TICK_MS;
            if (transition_elapsed_ms >= NB_APP_MAIN_FACE_DEMO_TRANSITION_MS) {
                transitioning = false;
            }
        } else {
            current = *nb_face_core_get_expression(to_expr);
        }
        current.open_l *= idle_out.open_l;
        current.open_r *= idle_out.open_r;

        nb_face_renderer_shell_draw(&current, idle_out.gaze_x, idle_out.gaze_y, idle_out.width_l,
                                    idle_out.width_r, idle_out.tilt, 0xffffffU);

        esp_err_t err = nb_display_hal_shell_flush_and_swap();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "display flush falhou (%s)", esp_err_to_name(err));
        }
        err = nb_face_renderer_shell_bind_buffer(nb_display_hal_shell_get_back_buffer());
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "bind do back buffer falhou (%s)", esp_err_to_name(err));
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
