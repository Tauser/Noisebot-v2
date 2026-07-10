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
 * S3.8, item 4 (primeira casca de arco, RFC-VIDA-V2.md §5.2):
 * GRUMPY_FORGIVE liga a nb_reflex_engine_shell_set_touch_sink() (novo
 * observador do bus, sem abrir um segundo leitor) pra contar TAP; o beat
 * ativo sobrepõe `current` (blend pra ANGRY/HAPPY, blink+gaze desviado)
 * depois da supressão de idle já existente. Sem abort ligado a IDLE
 * (bug real de bancada, 2026-07-08 -- TOUCH_REACTING volta a IDLE quase
 * instantâneo ao soltar o dedo, matava o arco cedo demais); termina só
 * pelo próprio ciclo + cooldown.
 * S3.8, item 5: RECONCILE liga direto em nb_touch_service_shell_is_
 * pressed()/get_duration_ms() (mesma consulta que reflex_engine_shell já
 * faz, não é bus) pro gatilho de carinho sustentado -- gatilho por
 * condição de nível, decidido a cada frame, sem hook de IDLE (mesma
 * lição do item 4: o próprio gate de carinho já limpa sozinho quando o
 * toque solta).
 * S3.8, item 6: SEARCH liga só o gatilho de tédio (decisão do usuário,
 * 2026-07-08 -- sem gatilho de toque direto ainda, RFC não especifica
 * qual). O touch_sink agora também rearma um contador de "ms desde o
 * último toque" a cada estímulo (não só TAP) -- fecha de verdade o
 * gancho de nb_idle_engine_set_energy_inputs() exposto desde o S3.7
 * (item 2, nunca chamado por nenhuma casca) e alimenta
 * nb_search_trigger_boredom() no mesmo teto de ~5min sem estímulo já
 * documentado em nb_energy.c. Beat SEARCHING não tem overlay próprio
 * ainda (calcular os padrões LATERAL/DIAGONAL/ORBIT é extensão do motor
 * de atenção, fora de escopo deste item -- deixado pro idle_engine
 * existente continuar).
 * S3.8, item 7 (emenda 2026-07-08, normativa do usuário): só 3 mecanismos
 * ligados -- HEART (beat CONTENT_SLOW_BLINK_HEART do RECONCILE), TEARS
 * (nb_peak_tears_trigger_tick(), histerese 0.70/0.60 sobre
 * dominant_hub==SAD), ZZZ (presente sse FSM==SLEEPING). Prioridade: arco
 * (HEART) > vetor (TEARS) > estado (ZZZ). LAUGH e os adornos ficam sem
 * casca (aguardam arco de gargalhada / EUREKA-CONFUSED do S4.8 / tag de
 * STIMULUS da mente). Sem pipeline de assets SVG->PBM ainda -- picos não
 * têm efeito visual de verdade nesta fatia, só log de ativação pra
 * observabilidade em bancada.
 * S3.8, item 8 (emenda 2026-07-08, normativa do usuário): gatilho por
 * processo de Poisson (hazard rate) dentro do próprio núcleo -- SNEEZE
 * elegível em IDLE acordado sem quiet_mode; DREAM em SLEEPING; STARGAZE
 * em NIGHT + IDLE acordado. Só log de ativação (ESP_LOGI) pra
 * observabilidade -- envio de STATUS com os contadores é pendência
 * separada (não construído; mind_link_shell não envia STATUS nenhum
 * hoje, ver ROADMAP.md).
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
#include "nb_app_config_shell.h"
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
#include "nb_audio_playback_service_shell.h"
#include "nb_wake_service_shell.h"
#include "audio_hal.h"
#include "nb_hw_config.h"
#include "idle_engine.h"
#include "emotion_core.h"
#include "tiny_fsm.h"
#include "grumpy_forgive.h"
#include "reconcile.h"
#include "search.h"
#include "peak_core.h"
#include "rarity_core.h"
#include <math.h>
#include <string.h>

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

/* S3.8, item 4: pesos práticos de blend pro overlay visual dos beats de
 * GRUMPY_FORGIVE (RFC não dá números -- retunar em bancada, mesma nota
 * das variantes do S3.7). "ANGRY ~60%" já vem do próprio RFC. */
#define NB_APP_MAIN_GRUMPY_ANGRY_BLEND 0.60f
#define NB_APP_MAIN_GRUMPY_HAPPY_BLEND 0.30f
#define NB_APP_MAIN_GRUMPY_GAZE_AWAY_OFFSET 0.15f

/* S3.8, item 5: espelha NB_REFLEX_TOUCH_CARESS_MS (reflex_engine.c,
 * #define privado) -- não importado, núcleos autonômicos irmãos não se
 * acoplam (ARCHITECTURE.md §2); mesmo valor prático, "carinho" = toque
 * sustentado por >=15s. */
#define NB_APP_MAIN_RECONCILE_CARESS_MIN_MS 15000u

/* S3.8, item 6: mesmo teto de "~5 min sem estímulo" já documentado em
 * nb_energy.c (motor de energia, S3.7 item 2) -- reaproveitado como
 * limiar de tédio pro SEARCH, não um valor novo inventado. */
#define NB_APP_MAIN_SEARCH_BOREDOM_THRESHOLD_MS 300000u
#define NB_APP_MAIN_SEARCH_FOUND_HAPPY_BLEND 0.40f
#define NB_APP_MAIN_SEARCH_ORIENT_WIDEN 1.05f
#define NB_APP_MAIN_SEARCH_SIGH_DROOP 0.20f

static const char *TAG = "nb2";

static bool nb_app_main_is_quiet_mode_forced(void)
{
    uint32_t enabled = 0u;
    if (!nb_app_config_shell_get_u32(NB_CONFIG_KEY_QUIET_MODE_ENABLED, &enabled)) {
        return false;
    }
    return enabled != 0u;
}

static nb_voice_audio_level_t nb_app_main_voice_level_from_rms(float rms)
{
    if (rms >= 1800.0f) {
        return NB_VOICE_AUDIO_LEVEL_LOUD;
    }
    if (rms >= 700.0f) {
        return NB_VOICE_AUDIO_LEVEL_SOFT;
    }
    return NB_VOICE_AUDIO_LEVEL_NONE;
}

static void nb_app_main_voice_sink(const nb_wake_output_t *out, const int16_t *pcm,
                                   uint32_t samples, float wake_score, void *ctx)
{
    (void)ctx;

    if (out == NULL) {
        return;
    }

    switch (out->action) {
    case NB_WAKE_ACTION_SESSION_ARMED:
        (void)nb_mind_link_shell_notify_event_wake(wake_score);
        break;
    case NB_WAKE_ACTION_LISTEN_BEGIN_WITH_AUDIO:
        (void)nb_mind_link_shell_notify_listen_start(out->session_id, NB_WAKE_SERVICE_SAMPLE_RATE);
        (void)nb_mind_link_shell_notify_listen_audio(out->session_id, pcm, samples);
        break;
    case NB_WAKE_ACTION_LISTEN_AUDIO:
        (void)nb_mind_link_shell_notify_listen_audio(out->session_id, pcm, samples);
        break;
    case NB_WAKE_ACTION_LISTEN_END:
        (void)nb_mind_link_shell_notify_listen_end(out->session_id);
        break;
    case NB_WAKE_ACTION_FEEDBACK:
    case NB_WAKE_ACTION_NONE:
    default:
        break;
    }
}

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

/* S3.8, itens 4 e 6: único observador do touch_sink de reflex_engine_shell
 * (só um callback cabe por vez -- nb_reflex_engine_shell_set_touch_sink()
 * substitui, não empilha). Conta TAP pro GRUMPY_FORGIVE (RFC: "≥3 TAP em
 * 10s") e rearma o "ms desde o último toque" pro motor de tédio (qualquer
 * estímulo de toque conta, não só TAP -- "tédio" é ausência de qualquer
 * contato). ctx aponta pros dois campos, ambos donos da face_demo_task
 * (chamado de dentro dela, síncrono, sem concorrência entre tasks). */
typedef struct {
    nb_grumpy_forgive_state_t *grumpy;
    uint32_t *last_touch_ms;
} nb_app_main_touch_sink_ctx_t;

static void nb_app_main_touch_sink(nb_reflex_stimulus_t stimulus, uint32_t timestamp_ms,
                                   void *ctx)
{
    nb_app_main_touch_sink_ctx_t *sink_ctx = (nb_app_main_touch_sink_ctx_t *)ctx;
    *sink_ctx->last_touch_ms = timestamp_ms;
    if (stimulus == NB_REFLEX_STIMULUS_TOUCH_TAP) {
        (void)nb_grumpy_forgive_on_tap(sink_ctx->grumpy, (uint64_t)timestamp_ms);
    }
}

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
    nb_grumpy_forgive_state_t grumpy;
    nb_reconcile_state_t reconcile;
    nb_search_state_t search;
    nb_peak_state_t peak;
    nb_peak_tears_trigger_t tears_trigger;
    nb_rarity_state_t rarity;
    nb_peak_mechanism_t prev_peak_mechanism = NB_PEAK_MECHANISM_NONE;
    uint32_t last_touch_ms = 0;
    nb_app_main_touch_sink_ctx_t touch_sink_ctx;
    uint32_t fps_frame_count = 0;
    uint16_t last_fps_x10 = 0u; /* S3.8, item 8: última medição, reusada no STATUS a cada frame */
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
    nb_fsm_state_t prev_fsm_state = NB_FSM_STATE_BOOT;

    (void)arg;

    nb_idle_engine_init(&idle, esp_random());
    nb_emotion_core_init(&emotion, esp_random());
    nb_tiny_fsm_init(&fsm);
    nb_tiny_fsm_apply_event(&fsm, NB_FSM_EVENT_BOOT_OK);
    nb_grumpy_forgive_init(&grumpy);
    nb_reconcile_init(&reconcile);
    nb_search_init(&search, esp_random());
    last_touch_ms = (uint32_t)(esp_timer_get_time() / 1000);
    touch_sink_ctx.grumpy = &grumpy;
    touch_sink_ctx.last_touch_ms = &last_touch_ms;
    nb_reflex_engine_shell_set_touch_sink(nb_app_main_touch_sink, &touch_sink_ctx);
    nb_peak_core_init(&peak);
    nb_peak_tears_trigger_init(&tears_trigger);
    nb_rarity_core_init(&rarity, esp_random());

    for (;;) {
        nb_idle_output_t idle_out;
        int64_t t0 = esp_timer_get_time();

        const nb_circadian_output_t circadian =
            nb_circadian_core_shell_tick(NB_APP_MAIN_FACE_DEMO_TICK_MS, &fsm);
        const bool effective_quiet_mode = circadian.quiet_mode || nb_app_main_is_quiet_mode_forced();
        nb_idle_engine_set_mode(&idle, effective_quiet_mode, NB_IDLE_ATTENTION_IDLE);

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

        /* S3.8, item 4: on_tap() (via touch_sink acima) já decidiu se o
         * arco inicia; aqui só avança o tempo. Bug real achado em
         * bancada (2026-07-08): checar "FSM==IDLE -> abort()" a cada
         * frame parecia seguro (abort() em IDLE já é no-op no arc_core),
         * mas NB_FSM_STATE_TOUCH_REACTING volta pra IDLE via
         * NB_FSM_EVENT_TOUCH_END assim que o dedo solta -- quase
         * instantâneo, não é a "saída de verdade" que H7 pressupõe. Isso
         * abortava o arco no frame seguinte ao 3º TAP, antes de qualquer
         * beat aparecer. Removido até existir um sinal de IDLE "estável"
         * de verdade (mesma lacuna documentada do
         * nb_idle_engine_reset_transient(), gancho exposto e ainda não
         * chamado por nenhuma casca desde o S3.7) -- o arco por enquanto
         * só termina pelo próprio ciclo natural (~2.4s) + cooldown. */
        nb_grumpy_forgive_tick(&grumpy, NB_APP_MAIN_FACE_DEMO_TICK_MS);

        /* S3.8, item 5: gatilho por condição de nível (vetor negativo +
         * carinho sustentado) -- mesma consulta direta a touch_service
         * que reflex_engine_shell já faz internamente (não é o bus, sem
         * risco de leitor duplicado). */
        const bool reconcile_valence_negative = emotion.valence < 0.0f;
        const bool reconcile_caress_active =
            nb_touch_service_shell_is_pressed() &&
            nb_touch_service_shell_get_duration_ms() >= NB_APP_MAIN_RECONCILE_CARESS_MIN_MS;
        nb_reconcile_tick(&reconcile, NB_APP_MAIN_FACE_DEMO_TICK_MS, reconcile_valence_negative,
                          reconcile_caress_active);

        /* S3.8, item 6: "ms desde o último toque" -- fecha de vez o
         * gancho de energia exposto desde o S3.7 item 2 (nunca chamado
         * até aqui) e alimenta o gatilho de tédio do SEARCH no mesmo
         * teto de ~5min já documentado em nb_energy.c. */
        const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
        const uint32_t boredom_ms = now_ms - last_touch_ms;
        nb_idle_engine_set_energy_inputs(&idle, boredom_ms, emotion.arousal);
        nb_search_tick(&search, NB_APP_MAIN_FACE_DEMO_TICK_MS);
        if (boredom_ms >= NB_APP_MAIN_SEARCH_BOREDOM_THRESHOLD_MS) {
            (void)nb_search_trigger_boredom(&search, (uint64_t)now_ms);
        }

        /* S3.8, item 8: elegibilidade decidida aqui (a casca sabe de
         * fsm/circadian_core, o núcleo não) -- SNEEZE em IDLE acordado
         * sem quiet; DREAM em SLEEPING; STARGAZE em NIGHT + IDLE
         * acordado. Só log de ativação -- envio de STATUS é pendência
         * separada (ver ROADMAP.md). */
        const nb_fsm_state_t rarity_fsm_state = nb_tiny_fsm_get_state(&fsm);
        const bool rarity_sneeze_eligible =
            rarity_fsm_state == NB_FSM_STATE_IDLE && !effective_quiet_mode;
        const bool rarity_is_sleeping = rarity_fsm_state == NB_FSM_STATE_SLEEPING;
        const bool rarity_stargaze_eligible =
            circadian.phase == NB_CIRCADIAN_PHASE_NIGHT && rarity_fsm_state == NB_FSM_STATE_IDLE;

        if (nb_rarity_core_tick_sneeze(&rarity, NB_APP_MAIN_FACE_DEMO_TICK_MS,
                                       rarity_sneeze_eligible, (uint64_t)now_ms)) {
            ESP_LOGI(TAG, "rarity: SNEEZE (total=%u)",
                    (unsigned)nb_rarity_core_count(&rarity, NB_RARITY_SNEEZE));
        }
        if (nb_rarity_core_tick_dream(&rarity, NB_APP_MAIN_FACE_DEMO_TICK_MS,
                                      rarity_is_sleeping)) {
            ESP_LOGI(TAG, "rarity: DREAM (total=%u)",
                    (unsigned)nb_rarity_core_count(&rarity, NB_RARITY_DREAM));
        }
        if (nb_rarity_core_tick_stargaze(&rarity, NB_APP_MAIN_FACE_DEMO_TICK_MS,
                                         rarity_stargaze_eligible)) {
            ESP_LOGI(TAG, "rarity: STARGAZE (total=%u)",
                    (unsigned)nb_rarity_core_count(&rarity, NB_RARITY_STARGAZE));
        }

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

        /* S3.8, item 7 (emenda 2026-07-08): TEARS avaliado sempre (mantém
         * a histerese consistente mesmo quando HEART vence a arbitragem
         * neste frame); prioridade arco > vetor > estado. Sem efeito
         * visual ainda (sem assets) -- log de ativação só pra
         * observabilidade em bancada. */
        const float peak_intensity =
            sqrtf(emotion.valence * emotion.valence + emotion.arousal * emotion.arousal);
        const bool peak_is_sad_dominant =
            emotion.has_dominant_hub && emotion.dominant_hub == NB_FACE_EXPR_SAD;
        const bool peak_tears_requested =
            nb_peak_tears_trigger_tick(&tears_trigger, peak_is_sad_dominant, peak_intensity);

        nb_peak_mechanism_t peak_requested = NB_PEAK_MECHANISM_NONE;
        if (nb_reconcile_current_beat(&reconcile) == NB_RECONCILE_BEAT_CONTENT_SLOW_BLINK_HEART) {
            peak_requested = NB_PEAK_MECHANISM_HEART;
        } else if (peak_tears_requested) {
            peak_requested = NB_PEAK_MECHANISM_TEARS;
        } else if (nb_tiny_fsm_get_state(&fsm) == NB_FSM_STATE_SLEEPING) {
            peak_requested = NB_PEAK_MECHANISM_ZZZ;
        }
        nb_peak_core_tick(&peak, NB_APP_MAIN_FACE_DEMO_TICK_MS, peak_requested);
        /* H7 é a TRANSIÇÃO X->IDLE, não "estar em IDLE" -- IDLE é o
         * estado padrão de repouso; resetar a cada frame em que
         * fsm==IDLE mataria TEARS (puramente dirigido por vetor, nasce
         * com o corpo já parado em IDLE na maioria das vezes) assim que
         * ele começasse. Mesma lição do bug do GRUMPY_FORGIVE, aqui pega
         * antes de flashear: borda de verdade (prev!=IDLE &&
         * atual==IDLE), não nível. */
        const nb_fsm_state_t current_fsm_state = nb_tiny_fsm_get_state(&fsm);
        if (current_fsm_state == NB_FSM_STATE_IDLE && prev_fsm_state != NB_FSM_STATE_IDLE) {
            nb_peak_core_reset_transient(&peak);
        }
        prev_fsm_state = current_fsm_state;
        const nb_peak_mechanism_t peak_active = nb_peak_core_active_mechanism(&peak);
        if (peak_active != prev_peak_mechanism) {
            ESP_LOGI(TAG, "peak: mechanism=%d", (int)peak_active);
            prev_peak_mechanism = peak_active;
        }

        /* P0-P3 (safety/touch/fala/hint) suprimem os motifs de idle sem
         * destruí-los -- ao expirar a claim, o overlay volta sozinho
         * (BEHAVIOR.md §3). */
        if (active_priority > NB_REFLEX_PRIORITY_HINT) {
            current.open_l *= idle_out.open_l;
            current.open_r *= idle_out.open_r;
        }

        /* S3.8, item 4: beat de GRUMPY_FORGIVE sobrepõe `current` por
         * cima de tudo -- overlay temporário (não escreve em emotion_core,
         * mesmo espírito do overlay de idle acima: nada de estado
         * duplicado/permanente, some sozinho quando o arco termina ou
         * aborta). Pesos/offset são valores práticos (RFC não dá números
         * pra este blend) -- retunar em bancada. */
        switch (nb_grumpy_forgive_current_beat(&grumpy)) {
        case NB_GRUMPY_FORGIVE_BEAT_ANGRY: {
            const nb_face_state_t *angry = nb_face_core_get_expression(NB_FACE_EXPR_ANGRY);
            nb_face_core_lerp(&current, angry, NB_APP_MAIN_GRUMPY_ANGRY_BLEND, &current);
            break;
        }
        case NB_GRUMPY_FORGIVE_BEAT_BLINK_GAZE_AWAY: {
            /* Blink lento (fecha na 1ª metade dos 800ms, reabre na 2ª --
             * envelope triangular) + gaze desviado (offset horizontal). */
            const uint64_t elapsed_ms = grumpy.arc.now_ms - grumpy.arc.phase_started_ms;
            const float t = (float)elapsed_ms / (float)NB_GRUMPY_FORGIVE_GAZE_MS;
            const float blink_close = (t < 0.5f) ? (t / 0.5f) : (1.0f - (t - 0.5f) / 0.5f);
            current.open_l *= (1.0f - blink_close);
            current.open_r *= (1.0f - blink_close);
            current.x_off += NB_APP_MAIN_GRUMPY_GAZE_AWAY_OFFSET;
            break;
        }
        case NB_GRUMPY_FORGIVE_BEAT_NEUTRAL_HAPPY: {
            const nb_face_state_t *happy = nb_face_core_get_expression(NB_FACE_EXPR_HAPPY);
            nb_face_core_lerp(&current, happy, NB_APP_MAIN_GRUMPY_HAPPY_BLEND, &current);
            break;
        }
        case NB_GRUMPY_FORGIVE_BEAT_NONE:
        default:
            break;
        }

        /* S3.8, item 5: beat de RECONCILE sobrepõe `current` por cima do
         * de GRUMPY_FORGIVE (mesmo espírito de overlay temporário -- os
         * dois arcos não disparam ao mesmo tempo na prática, já que um
         * pede vetor negativo+carinho e o outro TAP, mas não há exclusão
         * explícita entre eles ainda; se algum dia colidirem, o último
         * overlay aplicado vence). Glifo `♥` do item 7 (peak_core) ainda
         * não está ligado -- BEAT_..._HEART fica visualmente igual ao
         * beat sem coração por enquanto. */
        switch (nb_reconcile_current_beat(&reconcile)) {
        case NB_RECONCILE_BEAT_GAZE_FRONT: {
            const uint64_t elapsed_ms = reconcile.arc.now_ms - reconcile.arc.phase_started_ms;
            const float t = nb_face_core_clampf((float)elapsed_ms / (float)NB_RECONCILE_ORIENT_MS,
                                                0.0f, 1.0f);
            current.x_off = current.x_off * (1.0f - t); /* "gaze busca a frente" */
            break;
        }
        case NB_RECONCILE_BEAT_SMOOTHING: {
            const uint64_t elapsed_ms = reconcile.arc.now_ms - reconcile.arc.phase_started_ms;
            const float t = nb_face_core_clampf((float)elapsed_ms / (float)NB_RECONCILE_EXECUTE_MS,
                                                0.0f, 1.0f);
            const nb_face_state_t *content = nb_face_core_get_expression(NB_FACE_EXPR_CONTENT);
            nb_face_core_lerp(&current, content, t, &current);
            break;
        }
        case NB_RECONCILE_BEAT_CONTENT_SLOW_BLINK:
        case NB_RECONCILE_BEAT_CONTENT_SLOW_BLINK_HEART: {
            const nb_face_state_t *content = nb_face_core_get_expression(NB_FACE_EXPR_CONTENT);
            nb_face_core_lerp(&current, content, 1.0f, &current);
            const uint64_t elapsed_ms = reconcile.arc.now_ms - reconcile.arc.phase_started_ms;
            const float t = (float)elapsed_ms / (float)NB_RECONCILE_OUTCOME_MS;
            const float blink_close = (t < 0.5f) ? (t / 0.5f) : (1.0f - (t - 0.5f) / 0.5f);
            current.open_l *= (1.0f - blink_close);
            current.open_r *= (1.0f - blink_close);
            break;
        }
        case NB_RECONCILE_BEAT_NONE:
        default:
            break;
        }

        /* S3.8, item 6: overlay do SEARCH -- SEARCHING não tem overlay
         * próprio (padrões de vistada ficam pro idle_engine existente,
         * fora de escopo aqui, ver nota no topo do arquivo). */
        switch (nb_search_current_beat(&search)) {
        case NB_SEARCH_BEAT_ORIENT: {
            /* "perk" -- leve abertura extra, RFC não dá número. */
            current.open_l =
                nb_face_core_clampf(current.open_l * NB_APP_MAIN_SEARCH_ORIENT_WIDEN, 0.0f, 1.0f);
            current.open_r =
                nb_face_core_clampf(current.open_r * NB_APP_MAIN_SEARCH_ORIENT_WIDEN, 0.0f, 1.0f);
            break;
        }
        case NB_SEARCH_BEAT_FOUND: {
            const nb_face_state_t *happy = nb_face_core_get_expression(NB_FACE_EXPR_HAPPY);
            nb_face_core_lerp(&current, happy, NB_APP_MAIN_SEARCH_FOUND_HAPPY_BLEND, &current);
            break;
        }
        case NB_SEARCH_BEAT_NOT_FOUND_BLINK: {
            const uint64_t elapsed_ms = search.arc.now_ms - search.arc.phase_started_ms;
            const float t = (float)elapsed_ms / (float)NB_SEARCH_NOT_FOUND_BLINK_MS;
            const float blink_close = (t < 0.5f) ? (t / 0.5f) : (1.0f - (t - 0.5f) / 0.5f);
            current.open_l *= (1.0f - blink_close);
            current.open_r *= (1.0f - blink_close);
            break;
        }
        case NB_SEARCH_BEAT_NOT_FOUND_SIGH: {
            /* "gaze desce" -- mesma ideia do motif SIGH do idle_engine
             * (S3.7), aproximada aqui via y_l/y_r (offset vertical). */
            const uint64_t elapsed_ms = search.arc.now_ms - search.arc.phase_started_ms;
            const float t = nb_face_core_clampf(
                (float)elapsed_ms / (float)NB_SEARCH_NOT_FOUND_SIGH_MS, 0.0f, 1.0f);
            const float droop = (t < 0.5f) ? (t / 0.5f) : (1.0f - (t - 0.5f) / 0.5f);
            current.y_l += NB_APP_MAIN_SEARCH_SIGH_DROOP * droop;
            current.y_r += NB_APP_MAIN_SEARCH_SIGH_DROOP * droop;
            break;
        }
        case NB_SEARCH_BEAT_SEARCHING:
        case NB_SEARCH_BEAT_NONE:
        default:
            break;
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
            last_fps_x10 = (uint16_t)(fps * 10.0f);
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

        /* S3.8, item 8: snapshot de STATUS a cada frame -- fire-and-forget,
         * mind_link_shell reenvia o mais recente a cada HEARTBEAT (~1s,
         * decisão do usuário). bus_dropped soma normal+safety (ambos
         * indicam perda real, mesmo peso pro dashboard). */
        {
            nb_event_bus_stats_t bus_stats;
            nb_event_bus_shell_get_stats(&bus_stats);

            nbp2_msg_status_t status;
            memset(&status, 0, sizeof(status));
            status.state = (nbp2_fsm_state_t)nb_tiny_fsm_get_state(&fsm);
            status.valence = emotion.valence;
            status.arousal = emotion.arousal;
            status.heap_free = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
            status.psram_free = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
            status.fps_x10 = last_fps_x10;
            status.bus_dropped = bus_stats.dropped_normal + bus_stats.dropped_safety;
            status.rssi = nb_wifi_setup_shell_get_rssi();
            status.rarity_sneeze_count = nb_rarity_core_count(&rarity, NB_RARITY_SNEEZE);
            status.rarity_dream_count = nb_rarity_core_count(&rarity, NB_RARITY_DREAM);
            status.rarity_stargaze_count = nb_rarity_core_count(&rarity, NB_RARITY_STARGAZE);
            nb_mind_link_shell_notify_status(&status);
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
    static int16_t tx_buf[NB_APP_MAIN_AUDIO_TONE_SAMPLES];
    static int16_t mic_buf[NB_APP_MAIN_AUDIO_READ_CHUNK];
    nb_wake_service_shell_stats_t wake_stats;
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
        const bool quiet_mode_forced = nb_app_main_is_quiet_mode_forced();
        const nb_audio_playback_state_t playback_state =
            nb_audio_playback_service_shell_get_state();
        const size_t playback_samples =
            nb_audio_playback_service_shell_consume(tx_buf, NB_APP_MAIN_AUDIO_TONE_SAMPLES);
        const int16_t *tx_samples = tone;
        size_t tx_sample_count = NB_APP_MAIN_AUDIO_TONE_SAMPLES;

        if (playback_samples > 0u) {
            /* S4.3: enquanto houver SAY_* local pronto, o speaker toca o
             * playback vindo do mind_link; os samples restantes do bloco
             * são zerados pra não "vazar" o tom de bancada no meio da fala. */
            for (size_t i = playback_samples; i < NB_APP_MAIN_AUDIO_TONE_SAMPLES; ++i) {
                tx_buf[i] = 0;
            }
            tx_samples = tx_buf;
        } else if (playback_state != NB_AUDIO_PLAYBACK_STATE_IDLE) {
            /* Fade/turno ativo sem samples novos neste exato loop: envia
             * silêncio curto em vez do tom de bancada, preservando a
             * prioridade do playback local sobre o harness de S4.1. */
            for (size_t i = 0; i < NB_APP_MAIN_AUDIO_TONE_SAMPLES; ++i) {
                tx_buf[i] = 0;
            }
            tx_samples = tx_buf;
        }

        last_write_err = nb_audio_hal_shell_write(tx_samples, tx_sample_count,
                                                  NB_APP_MAIN_AUDIO_IO_TIMEOUT_MS, &written);

        size_t read_count = 0;
        last_read_err = nb_audio_hal_shell_read(mic_buf, NB_APP_MAIN_AUDIO_READ_CHUNK,
                                                NB_APP_MAIN_AUDIO_IO_TIMEOUT_MS, &read_count);
        ++loop_count;

        const int64_t now_us = esp_timer_get_time();
        nb_wake_service_shell_tick();
        const float rms = nb_audio_hal_rms_s16(mic_buf, read_count);
        const nb_voice_audio_level_t audio_level = nb_app_main_voice_level_from_rms(rms);
        if (read_count > 0u) {
#if CONFIG_NB_WAKE_BENCH_HARNESS
            const uint32_t now_ms = (uint32_t)(now_us / 1000);
            const bool speech_detected = rms >= (float)CONFIG_NB_WAKE_BENCH_VAD_RMS;

            if (!quiet_mode_forced &&
                nb_wake_service_shell_get_state() == NB_WAKE_STATE_IDLE &&
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
                nb_wake_service_shell_on_audio_frame(
                    mic_buf,
                    quiet_mode_forced ? false : speech_detected,
                    (uint32_t)read_count,
                    quiet_mode_forced ? NB_VOICE_AUDIO_LEVEL_NONE : audio_level);
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
                    "audio_bringup: mic_rms=%.1f playback_state=%d playback_buf=%u playback_drop=%u rx_ovf=%u tx_ovf=%u rx_timeout=%u tx_timeout=%u",
                    (double)rms, (int)nb_audio_playback_service_shell_get_state(),
                    (unsigned)nb_audio_playback_service_shell_get_buffered_samples(),
                    (unsigned)nb_audio_playback_service_shell_get_dropped_samples(),
                    (unsigned)nb_audio_hal_shell_get_rx_overflow_count(),
                    (unsigned)nb_audio_hal_shell_get_tx_overflow_count(),
                    (unsigned)nb_audio_hal_shell_get_rx_timeout_count(),
                    (unsigned)nb_audio_hal_shell_get_tx_timeout_count());
            nb_wake_service_shell_get_stats(&wake_stats);
            ESP_LOGI(
                TAG,
                "audio_bringup: wake_state=%d wakes=%u listen_start=%u last_latency_ms=%u max_latency_ms=%u budget_miss=%u fb_vad=%u fb_route=%u end_silence=%u end_max=%u",
                (int)nb_wake_service_shell_get_state(),
                (unsigned)wake_stats.wake_count,
                (unsigned)wake_stats.listen_start_count,
                (unsigned)wake_stats.last_listen_latency_ms,
                (unsigned)wake_stats.max_listen_latency_ms,
                (unsigned)wake_stats.listen_budget_miss_count,
                (unsigned)wake_stats.feedback_vad_unavailable_count,
                (unsigned)wake_stats.feedback_no_route_count,
                (unsigned)wake_stats.listen_end_silence_count,
                (unsigned)wake_stats.listen_end_max_duration_count);
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
        nb_wake_service_shell_set_voice_sink(nb_app_main_voice_sink, NULL);
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
        uint32_t persisted_volume_percent = 100u;
        ESP_ERROR_CHECK(nb_audio_playback_service_shell_init());
        if (nb_app_config_shell_get_u32(NB_CONFIG_KEY_AUDIO_VOLUME_PERCENT,
                                        &persisted_volume_percent)) {
            (void)nb_audio_playback_service_shell_set_volume_percent(persisted_volume_percent);
        }
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
