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
 * S2.6: instrumentação de fps (task de face) e heap/PSRAM (heartbeat)
 * pra registrar baseline no soak de 48h do gate visual da fase.
 * S3.1: touch_hal + touch_service (GPIO2/TOUCH2) numa task própria a 50Hz.
 * S3.2: reflex_engine liga touch_service a emotion_core/tiny_fsm --
 * touch_service_shell publica no event_bus (primeira casca real do bus,
 * S1.2), reflex_engine_shell drena e aplica delta afetivo + evento de
 * FSM. Pulso sintético de emotion_core (substituto do toque real) removido:
 * o vetor agora só recebe estímulo real de toque.
 * S3.3: led_service (WS2812 GPIO21, RMT) segue o estado do tiny_fsm a
 * cada frame; reflex_engine_shell dispara o overlay de toque no mesmo
 * lugar que já aplica em emotion_core/tiny_fsm.
 * S3.4: circadian_core (fase NIGHT/DAWN/DAY/DUSK, relógio de bancada
 * acelerado até TIME_SYNC real existir) alimenta led_service.brightness_
 * scale e idle_engine.quiet_mode a cada frame; dispara SLEEP/WAKE_HOUR em
 * tiny_fsm nas bordas de fase. mind_link_shell publica TIME_SYNC no
 * event_bus; reflex_engine_shell (único leitor do bus) despacha pro
 * circadian_core_shell.
 * S3.5: schedule_core (timers/alarmes locais, NVS) tickado por frame;
 * cada disparo aplica NB_REFLEX_STIMULUS_TIMER_FIRED (reflexo local:
 * emoção + overlay de LED) e pede EVENT_TIMER_FIRED ao mind_link
 * (fire-and-forget, sem bloquear se o server estiver offline).
 * mind_link_shell decodifica TIMER_SET/TIMER_CANCEL e publica no
 * event_bus; reflex_engine_shell despacha pro schedule_core_shell.
 * S4.1: audio_hal (I2S full-duplex 16kHz, GPIO39-42) numa task de
 * bring-up temporária -- toca um tom de teste continuamente enquanto lê
 * o mic, loga RMS + contadores de overflow/timeout periodicamente.
 * Substituída quando audio_service/wake_service existirem (S4.2+).
 * S3.7 completo, item 3 (acoplamentos, RFC-VIDA-V2.md §7): brilho do LED
 * idle passa a ser multiplicado também por idle_out.breath_scale (mesmo
 * fator que já modula a abertura dos olhos) -- respiração e LED em fase,
 * mesmo clock, sem estado duplicado. Única mudança fora do idle_engine
 * prevista pro item 3 (os demais acoplamentos -- blink×sacada, roll
 * segue gaze -- ficam inteiros dentro do núcleo).
 * S3.7 completo, item 6 (campo contínuo, RFC-VIDA-V2.md §3): o
 * nearest-neighbor + transição de 220 ms de S2.2/S2.5 dá lugar a
 * nb_emotion_core_resolve_face() -- blend contínuo entre os 4 hubs
 * (NEUTRAL/HAPPY/SAD/ANGRY; as outras 6 âncoras saem de uso, decisão de
 * produto 2026-07-06). S2.2 deixa de ser critério de paridade a partir
 * daqui (já previsto no ROADMAP).
 */

#include <stdbool.h>

#include "boot_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "nb_boot_manager_shell.h"
#include "nb_watchdog_shell.h"
#include "nb_wifi_setup_shell.h"
#include "nb_ota_shell.h"
#include "nb_mind_link_shell.h"
#include "nb_display_hal_shell.h"
#include "nb_face_renderer_shell.h"
#include "nb_touch_service_shell.h"
#include "nb_event_bus_shell.h"
#include "nb_reflex_engine_shell.h"
#include "nb_led_service_shell.h"
#include "nb_circadian_core_shell.h"
#include "nb_schedule_core_shell.h"
#include "nb_audio_hal_shell.h"
#include "nb_wake_service_shell.h"
#include "audio_hal.h"
#include "nb_hw_config.h"
#include "idle_engine.h"
#include "emotion_core.h"
#include "tiny_fsm.h"
#include <math.h>

#define NB_APP_MAIN_WATCHDOG_TIMEOUT_MS 10000u
#define NB_APP_MAIN_HEARTBEAT_MS 1000u
#define NB_APP_MAIN_STATS_LOG_EVERY_N_HEARTBEATS 60u /* ~60s -- soak de 48h sem inundar o log */
#define NB_APP_MAIN_FACE_DEMO_STACK 4096u
#define NB_APP_MAIN_FACE_DEMO_PRIO 8
#define NB_APP_MAIN_FACE_DEMO_TICK_MS 33u
#define NB_APP_MAIN_FPS_LOG_WINDOW_US 5000000ll /* janela de 5s pro fps medido */
#define NB_APP_MAIN_LED_BRIGHTNESS_CAP 0.15f
#define NB_APP_MAIN_TOUCH_STACK 3072u
#define NB_APP_MAIN_TOUCH_PRIO 8
#define NB_APP_MAIN_TOUCH_TICK_MS 20u /* 50Hz, mesma cadência do v1 */
#define NB_APP_MAIN_AUDIO_STACK 4096u
#define NB_APP_MAIN_AUDIO_PRIO 18 /* VOICE.md §6: audio_io > wake/vad(16) > mind_link(12) > render(8) */
#define NB_APP_MAIN_AUDIO_TONE_HZ 500u
/* Causa raiz real do bug de bancada (N32R16V, 2026-07-06): o buffer de
 * escrita tem que fornecer pelo menos tanto áudio quanto o loop consome
 * em tempo real por iteração, senão o TX passa fome (starvation) a cada
 * volta -- 128 amostras (8ms) por loop de ~16ms (pautado pelo read() de
 * 256 amostras) alimentava metade do necessário, gerando tx_ovf crescente
 * quase 1-pra-1 com cada iteração e contenção real com o SPI do display
 * (reduzir dma_desc_num/frame_num sozinho não resolvia -- o problema era
 * taxa de alimentação, não capacidade de buffer). Igual ao chunk de
 * leitura agora: 256 amostras = 8 períodos inteiros de 500Hz, sem clique. */
#define NB_APP_MAIN_AUDIO_TONE_SAMPLES 256u
#define NB_APP_MAIN_AUDIO_TONE_AMPLITUDE 8000.0f
#define NB_APP_MAIN_AUDIO_READ_CHUNK 256u /* VOICE.md §4: granularidade de streaming */
#define NB_APP_MAIN_AUDIO_IO_TIMEOUT_MS 200u
#define NB_APP_MAIN_AUDIO_LOG_PERIOD_US 5000000ll /* 5s, mesmo padrão do fps de face_demo */

static const char *TAG = "nb2";

#if CONFIG_NB_WAKE_BENCH_HARNESS
static float nb_app_main_clamp01(float x)
{
    if (x < 0.0f) {
        return 0.0f;
    }
    if (x > 1.0f) {
        return 1.0f;
    }
    return x;
}
#endif

/* idle_engine (S2.4) sobrepõe motifs de VISUAL.md §3 à expressão-âncora
 * mais próxima do vetor do emotion_core (S2.5). S3.2: reflex_engine_shell
 * drena o event_bus (toque real, publicado por touch_service_shell) e
 * aplica delta afetivo + evento de tiny_fsm a cada frame; enquanto uma
 * claim P0-P3 (safety/touch/fala/hint) está ativa, os motifs de idle (P5)
 * ficam suprimidos sem serem destruídos -- voltam sozinhos ao expirar. */
static void nb_app_main_face_demo_task(void *arg)
{
    nb_idle_engine_t idle;
    nb_emotion_state_t emotion;
    nb_tiny_fsm_t fsm;
    uint32_t fps_frame_count = 0;
    int64_t fps_window_start_us = esp_timer_get_time();
    int64_t logic_us_sum = 0;
    int64_t draw_us_sum = 0;
    int64_t flush_us_sum = 0;
    uint32_t flush_bytes_sum = 0;
    /* Loop de período fixo, não "trabalho + delay fixo" -- bug real medido
     * em bancada (S2.6): vTaskDelay(33ms) incondicional após ~11ms de
     * trabalho (desenho+flush) dava período real de ~44ms (~23 fps) em vez
     * dos 33ms (~30 fps) pretendidos. vTaskDelayUntil desconta o tempo já
     * gasto e mantém o período alvo. */
    TickType_t last_wake_tick = xTaskGetTickCount();

    (void)arg;

    nb_idle_engine_init(&idle, esp_random());
    nb_emotion_core_init(&emotion, esp_random());
    nb_tiny_fsm_init(&fsm);
    nb_tiny_fsm_apply_event(&fsm, NB_FSM_EVENT_BOOT_OK);

    for (;;) {
        nb_idle_output_t idle_out;
        int64_t t0 = esp_timer_get_time();

        const nb_circadian_output_t circadian =
            nb_circadian_core_shell_tick(NB_APP_MAIN_FACE_DEMO_TICK_MS, &fsm);
        nb_idle_engine_set_mode(&idle, circadian.quiet_mode, NB_IDLE_ATTENTION_IDLE);

        nb_emotion_core_tick(&emotion, NB_APP_MAIN_FACE_DEMO_TICK_MS);
        nb_idle_engine_tick(&idle, NB_APP_MAIN_FACE_DEMO_TICK_MS, &idle_out);
        /* Teto de brilho em 15% -- pedido do usuário, 2026-07-05. Mantém a
         * variação circadiana (mais escuro à noite) mas nunca passa de 15%
         * do brilho nominal do LED. S3.7 completo, item 3 (acoplamentos,
         * RFC-VIDA-V2.md §7 "respiração ... em fase com o LED idle"):
         * multiplica pelo mesmo breath_scale que já modula open_l/open_r
         * no idle_engine -- mesmo sinal, mesma fase, sem clock separado. */
        nb_led_service_shell_set_brightness_scale(circadian.brightness_scale *
                                                  NB_APP_MAIN_LED_BRIGHTNESS_CAP *
                                                  idle_out.breath_scale);
        const nb_reflex_priority_t active_priority =
            nb_reflex_engine_shell_tick(&emotion, &fsm);
        nb_led_service_shell_tick(nb_tiny_fsm_get_state(&fsm), NB_APP_MAIN_FACE_DEMO_TICK_MS);

        /* S3.5: cada timer disparado aplica o reflexo local (emoção +
         * overlay de LED, dentro de apply_stimulus) e pede o envio de
         * EVENT_TIMER_FIRED -- fire-and-forget, não bloqueia se o mind_link
         * não estiver READY. */
        uint32_t fired_timer_ids[NB_SCHEDULE_MAX_TIMERS];
        const uint32_t fired_count =
            nb_schedule_core_shell_tick(fired_timer_ids, NB_SCHEDULE_MAX_TIMERS);
        for (uint32_t i = 0; i < fired_count; ++i) {
            nb_reflex_engine_shell_apply_stimulus(NB_REFLEX_STIMULUS_TIMER_FIRED, &emotion, &fsm);
            nb_mind_link_shell_notify_timer_fired(fired_timer_ids[i]);
        }

        /* S3.7 completo, item 6 (RFC-VIDA-V2.md §3): campo contínuo entre
         * os 4 hubs substitui o nearest-neighbor + transição de 220ms --
         * o próprio vetor (v,a) já se move suavemente (decay/estímulo), o
         * blend por posição já entrega a face contínua sem precisar de um
         * timer de easing artificial. */
        nb_face_state_t current;
        nb_emotion_core_resolve_face(&emotion, &current);
        /* P0-P3 (safety/touch/fala/hint) suprimem os motifs de idle sem
         * destruí-los -- ao expirar a claim, o overlay volta sozinho
         * (BEHAVIOR.md §3). */
        if (active_priority > NB_REFLEX_PRIORITY_HINT) {
            current.open_l *= idle_out.open_l;
            current.open_r *= idle_out.open_r;
        }

        int64_t t1 = esp_timer_get_time();
        logic_us_sum += t1 - t0;

        const nb_display_hal_rect_t dirty =
            nb_face_renderer_shell_draw_dirty(&current, idle_out.gaze_x, idle_out.gaze_y,
                                              idle_out.width_l, idle_out.width_r,
                                              idle_out.tilt, 0xffffffU);

        int64_t t2 = esp_timer_get_time();
        draw_us_sum += t2 - t1;

        const nb_display_hal_rect_t flush_rect = nb_display_hal_rect_align_for_flush(dirty);
        flush_bytes_sum +=
            (uint32_t)flush_rect.w * (uint32_t)flush_rect.h * (uint32_t)sizeof(uint16_t);

        esp_err_t err = nb_display_hal_shell_flush_rect_and_swap(dirty);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "display flush falhou (%s)", esp_err_to_name(err));
        }
        err = nb_face_renderer_shell_bind_buffer(nb_display_hal_shell_get_back_buffer());
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "bind do back buffer falhou (%s)", esp_err_to_name(err));
        }

        int64_t t3 = esp_timer_get_time();
        flush_us_sum += t3 - t2;

        /* fps medido (S2.6): budget baseline do soak de 48h. */
        ++fps_frame_count;
        const int64_t now_us = t3;
        const int64_t elapsed_us = now_us - fps_window_start_us;
        if (elapsed_us >= NB_APP_MAIN_FPS_LOG_WINDOW_US) {
            const float fps = (float)fps_frame_count / ((float)elapsed_us / 1000000.0f);
            ESP_LOGI(TAG,
                    "face_demo: fps=%.1f logic_ms=%.2f draw_ms=%.2f flush_ms=%.2f flush_kb=%.1f",
                    (double)fps, (double)logic_us_sum / fps_frame_count / 1000.0,
                    (double)draw_us_sum / fps_frame_count / 1000.0,
                    (double)flush_us_sum / fps_frame_count / 1000.0,
                    (double)flush_bytes_sum / fps_frame_count / 1024.0);
            fps_frame_count = 0;
            fps_window_start_us = now_us;
            logic_us_sum = 0;
            draw_us_sum = 0;
            flush_us_sum = 0;
            flush_bytes_sum = 0;
        }

        vTaskDelayUntil(&last_wake_tick, pdMS_TO_TICKS(NB_APP_MAIN_FACE_DEMO_TICK_MS));
    }
}

/* S4.1: bring-up temporário do audio_hal -- toca um tom de 400Hz (período
 * inteiro em 40 amostras a 16kHz, sem clique de junção entre blocos)
 * continuamente enquanto lê o mic, logando RMS + contadores de
 * overflow/timeout a cada ~5s (mesma cadência do fps de face_demo).
 * Sem vTaskDelay: read/write bloqueantes já pautam o loop no ritmo real
 * do DMA I2S. Substituído quando audio_service/wake_service existirem. */
static void nb_app_main_audio_task(void *arg)
{
    static int16_t tone[NB_APP_MAIN_AUDIO_TONE_SAMPLES];
    static int16_t mic_buf[NB_APP_MAIN_AUDIO_READ_CHUNK];
    int64_t next_log_us;
#if CONFIG_NB_WAKE_BENCH_HARNESS
    uint32_t next_wake_allowed_ms = 0u;
#endif

    (void)arg;

    for (uint32_t i = 0; i < NB_APP_MAIN_AUDIO_TONE_SAMPLES; ++i) {
        /* M_PI não é garantido pelo newlib sem _GNU_SOURCE -- constante
         * local, mesma prática de idle_engine (NB_IDLE_PI). Fase pela
         * razão tone_hz/sample_rate (não pelo tamanho do buffer) -- dá
         * um número inteiro de períodos (4x 32 amostras) sem clique de
         * junção no fim do buffer. */
        const float pi = 3.14159265358979323846f;
        const float phase =
            2.0f * pi * (float)NB_APP_MAIN_AUDIO_TONE_HZ * (float)i / (float)NB_HW_AUDIO_SAMPLE_RATE_HZ;
        tone[i] = (int16_t)(NB_APP_MAIN_AUDIO_TONE_AMPLITUDE * sinf(phase));
    }

    next_log_us = esp_timer_get_time() + NB_APP_MAIN_AUDIO_LOG_PERIOD_US;

    esp_err_t last_write_err = ESP_OK;
    esp_err_t last_read_err = ESP_OK;
    uint32_t loop_count = 0;

    for (;;) {
        size_t written = 0;
        last_write_err = nb_audio_hal_shell_write(tone, NB_APP_MAIN_AUDIO_TONE_SAMPLES,
                                                  NB_APP_MAIN_AUDIO_IO_TIMEOUT_MS, &written);

        size_t read_count = 0;
        last_read_err = nb_audio_hal_shell_read(mic_buf, NB_APP_MAIN_AUDIO_READ_CHUNK,
                                                NB_APP_MAIN_AUDIO_IO_TIMEOUT_MS, &read_count);
        ++loop_count;

        const int64_t now_us = esp_timer_get_time();
        nb_wake_service_shell_tick();
        const float rms = nb_audio_hal_rms_s16(mic_buf, read_count);
        if (read_count > 0u) {
#if CONFIG_NB_WAKE_BENCH_HARNESS
            const uint32_t now_ms = (uint32_t)(now_us / 1000);
            const bool speech_detected = rms >= (float)CONFIG_NB_WAKE_BENCH_VAD_RMS;

            if (nb_wake_service_shell_get_state() == NB_WAKE_STATE_IDLE &&
                rms >= (float)CONFIG_NB_WAKE_BENCH_WAKE_RMS &&
                now_ms >= next_wake_allowed_ms) {
                const float score = nb_app_main_clamp01(
                    rms / ((float)CONFIG_NB_WAKE_BENCH_WAKE_RMS * 2.0f));
                nb_wake_service_shell_trigger_wake(score);
                next_wake_allowed_ms = now_ms + CONFIG_NB_WAKE_BENCH_COOLDOWN_MS;
                ESP_LOGI(TAG, "wake_bench: trigger rms=%.1f score=%.2f", (double)rms,
                         (double)score);
            }

            if (nb_wake_service_shell_get_state() != NB_WAKE_STATE_IDLE) {
                nb_wake_service_shell_on_audio_frame(speech_detected, (uint32_t)read_count);
            }
#endif
        }
        if (now_us >= next_log_us) {
            next_log_us = now_us + NB_APP_MAIN_AUDIO_LOG_PERIOD_US;
            ESP_LOGI(TAG, "audio_bringup: loops=%u write_err=%s(%d) written=%u read_err=%s(%d) read=%u",
                    (unsigned)loop_count, esp_err_to_name(last_write_err), (int)last_write_err,
                    (unsigned)written, esp_err_to_name(last_read_err), (int)last_read_err,
                    (unsigned)read_count);
            loop_count = 0;
            ESP_LOGI(TAG,
                    "audio_bringup: mic_rms=%.1f rx_ovf=%u tx_ovf=%u rx_timeout=%u tx_timeout=%u",
                    (double)rms, (unsigned)nb_audio_hal_shell_get_rx_overflow_count(),
                    (unsigned)nb_audio_hal_shell_get_tx_overflow_count(),
                    (unsigned)nb_audio_hal_shell_get_rx_timeout_count(),
                    (unsigned)nb_audio_hal_shell_get_tx_timeout_count());
        }
    }
}

/* touch_service (S3.1) a 50Hz -- publica no event_bus (S3.2, dentro do
 * shell), quem consome é reflex_engine_shell na task de face. */
static void nb_app_main_touch_task(void *arg)
{
    TickType_t last_wake_tick = xTaskGetTickCount();

    (void)arg;

    for (;;) {
        nb_touch_service_shell_update(NB_APP_MAIN_TOUCH_TICK_MS);
        vTaskDelayUntil(&last_wake_tick, pdMS_TO_TICKS(NB_APP_MAIN_TOUCH_TICK_MS));
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

    /* S3.2: event_bus antes de qualquer task que publique (touch) ou faça
     * poll (face/reflex). */
    ESP_ERROR_CHECK(nb_event_bus_shell_init());
    nb_reflex_engine_shell_init();

    esp_err_t led_err = nb_led_service_shell_init();
    if (led_err != ESP_OK) {
        ESP_LOGE(TAG, "led_service falhou (%s) -- seguindo sem LED", esp_err_to_name(led_err));
    }

    /* S3.4: brilho circadiano passa a vir do circadian_core a cada frame
     * (nb_app_main_face_demo_task), substituindo o valor fixo de 15%. */
    nb_circadian_core_shell_init();

    /* S3.5: timers/alarmes locais, persistidos em NVS. */
    nb_schedule_core_shell_init();

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

    /* S4.2: wake_service nasce como casca mínima (sem WakeNet/ESP-SR
     * reais ainda). Rota local ainda não existe; rota da mente é atualizada
     * dinamicamente no loop principal conforme o mind_link entra/sai de
     * READY. */
    esp_err_t wake_err = nb_wake_service_shell_init();
    if (wake_err != ESP_OK) {
        ESP_LOGE(TAG, "wake_service falhou (%s) -- seguindo sem wake", esp_err_to_name(wake_err));
    } else {
#if CONFIG_NB_WAKE_BENCH_HARNESS
        ESP_LOGW(TAG, "wake_bench_harness ativo -- RMS heuristico de bancada, nunca producao");
        nb_wake_service_shell_set_vad_available(true);
#else
        nb_wake_service_shell_set_vad_available(false);
#endif
        nb_wake_service_shell_set_routes(
            nb_mind_link_shell_get_state() == NB_MIND_LINK_STATE_READY, false);
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

    esp_err_t touch_err = nb_touch_service_shell_init();
    if (touch_err != ESP_OK) {
        ESP_LOGE(TAG, "touch_service falhou (%s) -- seguindo sem toque",
                 esp_err_to_name(touch_err));
    } else {
        xTaskCreate(nb_app_main_touch_task, "touch", NB_APP_MAIN_TOUCH_STACK, NULL,
                   NB_APP_MAIN_TOUCH_PRIO, NULL);
    }

    /* S4.1: audio_hal (I2S full-duplex) -- bring-up temporário até
     * audio_service/wake_service existirem. Bug real de bancada
     * (N32R16V, 2026-07-06): display cortando com ESP_ERR_NO_MEM
     * constante depois que este task subia -- causa raiz é contenção real
     * de SRAM interna DMA-capable entre o staging PSRAM->SRAM de 150 KB do
     * display full-frame e os descritores DMA do I2S deste componente --
     * exatamente a contenção render+I2S que o gate de S4.1 exige fechar
     * (re-valida S0.3). S4.1a reduz essa pressão com dirty rectangles e
     * staging DMA fixo de ~19 KB no display_hal; reduzir
     * dma_desc_num/dma_frame_num aqui (audio_hal_shell) fica como margem de
     * segurança de memória adicional. */
    esp_err_t audio_err = nb_audio_hal_shell_init();
    if (audio_err != ESP_OK) {
        ESP_LOGE(TAG, "audio_hal falhou (%s) -- seguindo sem audio", esp_err_to_name(audio_err));
    } else {
        xTaskCreate(nb_app_main_audio_task, "audio_bringup", NB_APP_MAIN_AUDIO_STACK, NULL,
                   NB_APP_MAIN_AUDIO_PRIO, NULL);
    }

    uint32_t heartbeat_count = 0;
    for (;;) {
        ESP_ERROR_CHECK(nb_watchdog_shell_feed());
        vTaskDelay(pdMS_TO_TICKS(NB_APP_MAIN_HEARTBEAT_MS));
        ESP_LOGI(TAG, "alive");
        nb_wake_service_shell_set_routes(
            nb_mind_link_shell_get_state() == NB_MIND_LINK_STATE_READY, false);

        /* Baseline de heap/PSRAM (S2.6): budget do soak de 48h. */
        ++heartbeat_count;
        if (heartbeat_count >= NB_APP_MAIN_STATS_LOG_EVERY_N_HEARTBEATS) {
            heartbeat_count = 0;
            ESP_LOGI(TAG,
                    "stats: heap_livre=%u heap_min=%u psram_livre=%u psram_min=%u",
                    (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                    (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
                    (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                    (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
        }
    }
}
