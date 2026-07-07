#include "nb_reflex_engine_shell.h"

#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "nb_circadian_core_shell.h"
#include "nb_event_bus_shell.h"
#include "nb_led_service_shell.h"
#include "nb_schedule_core_shell.h"
#include "nb_touch_service_shell.h"
#include "touch_service.h"

static const char *TAG = "reflex_engine";

static nb_reflex_engine_t s_engine;
static bool s_initialized;
static uint32_t s_last_now_ms;

void nb_reflex_engine_shell_init(void) {
    nb_reflex_engine_init(&s_engine);
    s_last_now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    s_initialized = true;
}

static nb_reflex_stimulus_t map_touch_event(nb_touch_event_t event) {
    switch (event) {
    case NB_TOUCH_EVENT_TAP:
        return NB_REFLEX_STIMULUS_TOUCH_TAP;
    case NB_TOUCH_EVENT_LONG_PRESS:
        return NB_REFLEX_STIMULUS_TOUCH_LONG;
    case NB_TOUCH_EVENT_SUSTAINED:
        return NB_REFLEX_STIMULUS_TOUCH_WARM_PULSE;
    case NB_TOUCH_EVENT_WAKE:
        return NB_REFLEX_STIMULUS_TOUCH_WAKE;
    default:
        return NB_REFLEX_STIMULUS_COUNT;
    }
}

static nb_reflex_stimulus_t map_voice_event(const nb_voice_event_payload_t *payload) {
    if (payload == NULL) {
        return NB_REFLEX_STIMULUS_COUNT;
    }

    switch ((nb_voice_event_kind_t)payload->kind) {
    case NB_VOICE_EVENT_WAKE:
        return NB_REFLEX_STIMULUS_VOICE_START;
    case NB_VOICE_EVENT_LISTEN_AUDIO:
        if ((nb_voice_audio_level_t)payload->detail == NB_VOICE_AUDIO_LEVEL_LOUD) {
            return NB_REFLEX_STIMULUS_VOICE_LOUD;
        }
        if ((nb_voice_audio_level_t)payload->detail == NB_VOICE_AUDIO_LEVEL_SOFT) {
            return NB_REFLEX_STIMULUS_VOICE_SOFT;
        }
        return NB_REFLEX_STIMULUS_COUNT;
    default:
        return NB_REFLEX_STIMULUS_COUNT;
    }
}

static void apply_reaction(const nb_reflex_reaction_t *reaction, nb_emotion_state_t *emotion,
                           nb_tiny_fsm_t *fsm) {
    if (reaction->has_affect_delta && emotion != NULL) {
        nb_emotion_core_apply_stimulus(emotion, reaction->delta_valence, reaction->delta_arousal);
    }
    if (reaction->fsm_event != NB_FSM_EVENT_COUNT && fsm != NULL) {
        nb_tiny_fsm_apply_event(fsm, reaction->fsm_event);
        if (nb_tiny_fsm_get_state(fsm) == NB_FSM_STATE_IDLE) {
            nb_reflex_engine_force_clear(&s_engine);
        }
    }
    /* S3.3: flash de toque no LED -- mesmo gatilho que já move
     * emotion_core/tiny_fsm (VISUAL.md §6, overlay ignorado sozinho pelo
     * núcleo de led_service se ERROR/SAFE_MODE estiver ativo). */
    if (reaction->fsm_event == NB_FSM_EVENT_TOUCH) {
        nb_led_service_shell_trigger_touch();
    }
}

void nb_reflex_engine_shell_apply_stimulus(nb_reflex_stimulus_t stimulus, nb_emotion_state_t *emotion,
                                           nb_tiny_fsm_t *fsm) {
    if (!s_initialized) {
        return;
    }
    const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    nb_reflex_reaction_t reaction;
    nb_reflex_engine_on_stimulus(&s_engine, stimulus, now_ms, &reaction);
    apply_reaction(&reaction, emotion, fsm);

    /* S3.5: disparo de timer reaproveita o overlay visual do toque (mesma
     * linguagem de VISUAL.md §6, sem overlay dedicado nesta fatia) -- só
     * aqui, uma vez por chamada, não a cada tick com a claim P3 ainda
     * ativa. */
    if (stimulus == NB_REFLEX_STIMULUS_TIMER_FIRED) {
        nb_led_service_shell_trigger_touch();
    }
}

nb_reflex_priority_t nb_reflex_engine_shell_tick(nb_emotion_state_t *emotion, nb_tiny_fsm_t *fsm) {
    if (!s_initialized) {
        return NB_REFLEX_PRIORITY_UNCLAIMED;
    }

    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    nb_reflex_reaction_t reaction;

    /* Único leitor do event_bus (FIFO de leitor único -- não dá pra ter
     * dois consumidores fazendo poll do mesmo bus, S3.4): também
     * despacha NB_EVENT_TYPE_TIME_SYNC pro circadian_core_shell (mesma
     * camada L4, chamada direta, não é cross-layer). */
    nb_event_t event;
    while (nb_event_bus_shell_poll(&event) == NB_EVENT_BUS_OK) {
        if (event.type == NB_EVENT_TYPE_TIME_SYNC && event.payload_len == sizeof(uint64_t)) {
            uint64_t unix_ms = 0u;
            memcpy(&unix_ms, event.payload, sizeof(unix_ms));
            nb_circadian_core_shell_apply_time_sync(unix_ms);
            continue;
        }

        if (event.type == NB_EVENT_TYPE_TIMER && event.payload_len == sizeof(nb_schedule_event_payload_t)) {
            nb_schedule_event_payload_t payload;
            memcpy(&payload, event.payload, sizeof(payload));
            if (payload.action == NB_SCHEDULE_EVENT_ACTION_SET) {
                nb_schedule_core_shell_handle_set(payload.timer_id, payload.fire_at_unix_ms);
            } else {
                nb_schedule_core_shell_handle_cancel(payload.timer_id);
            }
            continue;
        }

        if (event.type == NB_EVENT_TYPE_VOICE && event.payload_len == sizeof(nb_voice_event_payload_t)) {
            nb_voice_event_payload_t payload;
            memcpy(&payload, event.payload, sizeof(payload));
            nb_reflex_stimulus_t stimulus = map_voice_event(&payload);
            if (stimulus == NB_REFLEX_STIMULUS_COUNT) {
                continue;
            }
            nb_reflex_engine_on_stimulus(&s_engine, stimulus, event.timestamp_ms, &reaction);
            apply_reaction(&reaction, emotion, fsm);
            const uint32_t latency_ms = now_ms - event.timestamp_ms;
            ESP_LOGI(TAG, "voice kind=%u session=%u fsm_event=%d latency_ms=%u",
                     (unsigned)payload.kind, (unsigned)payload.session_id,
                     (int)reaction.fsm_event, (unsigned)latency_ms);
            continue;
        }

        if (event.type != NB_EVENT_TYPE_TOUCH ||
            event.payload_len != sizeof(nb_touch_event_payload_t)) {
            continue;
        }
        nb_touch_event_payload_t payload;
        memcpy(&payload, event.payload, sizeof(payload));
        nb_reflex_stimulus_t stimulus = map_touch_event(payload.event);
        if (stimulus == NB_REFLEX_STIMULUS_COUNT) {
            continue;
        }
        nb_reflex_engine_on_stimulus(&s_engine, stimulus, event.timestamp_ms, &reaction);
        apply_reaction(&reaction, emotion, fsm);

        /* Latência estímulo->reação (QUALITY.md: budget < 80ms p95) --
         * arrival no bus (touch_service_shell, esp_timer) até aplicação
         * aqui (mesma base de clock). Medida real de bancada, S3.2. */
        const uint32_t latency_ms = now_ms - event.timestamp_ms;
        ESP_LOGI(TAG, "reaction event=%d priority=%d fsm_event=%d latency_ms=%u",
                 (int)payload.event, (int)reaction.active_priority, (int)reaction.fsm_event,
                 (unsigned)latency_ms);
    }

    uint32_t dt_ms = now_ms - s_last_now_ms;
    s_last_now_ms = now_ms;

    bool pressed = nb_touch_service_shell_is_pressed();
    uint32_t duration_ms = nb_touch_service_shell_get_duration_ms();
    nb_reflex_engine_tick(&s_engine, dt_ms, pressed, duration_ms, &reaction);
    apply_reaction(&reaction, emotion, fsm);

    return reaction.active_priority;
}
