#include "nb_circadian_core_shell.h"

#include "esp_log.h"
#include "nb_app_config_shell.h"

static const char *TAG = "circadian_core";

/* Comprime 1 dia (86400s) em 6 minutos de bancada -- só demo, sem
 * servidor real enviando TIME_SYNC ainda (ver README do componente). */
#define NB_CIRCADIAN_SHELL_BENCH_ACCEL_FACTOR 240u

/* Sem horário salvo no NVS (primeiro boot): semeia 20h (início de DUSK)
 * pra sessão de bancada observar DUSK->NIGHT->DAWN->DAY rapidamente. */
#define NB_CIRCADIAN_SHELL_BOOTSTRAP_UNIX_S (20u * 3600u)

/* Persiste o horário estimado no NVS a cada ~30s de tempo real decorrido
 * (não acelerado) -- não precisa ser fino, é só o fallback de reboot. */
#define NB_CIRCADIAN_SHELL_PERSIST_PERIOD_MS 30000u

static nb_circadian_core_t s_core;
static bool s_initialized;
static nb_circadian_phase_t s_last_phase;
static bool s_pending_sleep;
static bool s_pending_wake;
static uint32_t s_since_persist_ms;

void nb_circadian_core_shell_init(void) {
    nb_circadian_core_init(&s_core);

    uint32_t last_known_s = 0u;
    nb_app_config_shell_get_u32(NB_CONFIG_KEY_LAST_KNOWN_UNIX_TIME_S, &last_known_s);

    if (last_known_s == 0u) {
        ESP_LOGW(TAG, "sem horario salvo -- semeando bancada em 20h (DUSK)");
        last_known_s = NB_CIRCADIAN_SHELL_BOOTSTRAP_UNIX_S;
    }
    nb_circadian_core_set_time_anchor(&s_core, (uint64_t)last_known_s * 1000u);

    s_last_phase = NB_CIRCADIAN_PHASE_DAY;
    s_pending_sleep = false;
    s_pending_wake = false;
    s_since_persist_ms = 0u;
    s_initialized = true;
}

uint64_t nb_circadian_core_shell_now_unix_ms(void) {
    if (!s_initialized) {
        return 0u;
    }
    return nb_circadian_core_now_unix_ms(&s_core);
}

void nb_circadian_core_shell_apply_time_sync(uint64_t unix_ms) {
    if (!s_initialized) {
        return;
    }
    nb_circadian_core_set_time_anchor(&s_core, unix_ms);
    ESP_LOGI(TAG, "TIME_SYNC aplicado -- unix_ms=%llu", (unsigned long long)unix_ms);
}

nb_circadian_output_t nb_circadian_core_shell_tick(uint32_t dt_ms, nb_tiny_fsm_t *fsm) {
    if (!s_initialized) {
        nb_circadian_output_t neutral;
        neutral.phase = NB_CIRCADIAN_PHASE_DAY;
        neutral.quiet_mode = false;
        neutral.brightness_scale = 1.0f;
        neutral.has_time_source = false;
        return neutral;
    }

    const nb_circadian_output_t out =
        nb_circadian_core_tick(&s_core, dt_ms * NB_CIRCADIAN_SHELL_BENCH_ACCEL_FACTOR);

    if (out.phase == NB_CIRCADIAN_PHASE_NIGHT && s_last_phase != NB_CIRCADIAN_PHASE_NIGHT) {
        s_pending_sleep = true;
    }
    if (out.phase == NB_CIRCADIAN_PHASE_DAWN && s_last_phase != NB_CIRCADIAN_PHASE_DAWN) {
        s_pending_wake = true;
    }
    s_last_phase = out.phase;

    if (fsm != NULL) {
        if (s_pending_sleep) {
            nb_tiny_fsm_apply_event(fsm, NB_FSM_EVENT_SLEEP);
            if (nb_tiny_fsm_get_state(fsm) == NB_FSM_STATE_SLEEPING) {
                s_pending_sleep = false;
                ESP_LOGI(TAG, "SLEEP aplicado -- entrando em NIGHT");
            }
        }
        if (s_pending_wake) {
            nb_tiny_fsm_apply_event(fsm, NB_FSM_EVENT_WAKE_HOUR);
            if (nb_tiny_fsm_get_state(fsm) != NB_FSM_STATE_SLEEPING) {
                s_pending_wake = false;
                ESP_LOGI(TAG, "WAKE_HOUR aplicado -- entrando em DAWN");
            }
        }
    }

    s_since_persist_ms += dt_ms;
    if (s_since_persist_ms >= NB_CIRCADIAN_SHELL_PERSIST_PERIOD_MS) {
        s_since_persist_ms = 0u;
        const uint64_t now_unix_ms = nb_circadian_core_now_unix_ms(&s_core);
        nb_app_config_shell_set_u32(NB_CONFIG_KEY_LAST_KNOWN_UNIX_TIME_S,
                                     (uint32_t)(now_unix_ms / 1000u));
    }

    return out;
}
