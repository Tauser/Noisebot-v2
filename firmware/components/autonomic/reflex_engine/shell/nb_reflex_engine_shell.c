#include "nb_reflex_engine_shell.h"

#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "nb_event_bus_shell.h"
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
}

nb_reflex_priority_t nb_reflex_engine_shell_tick(nb_emotion_state_t *emotion, nb_tiny_fsm_t *fsm) {
    if (!s_initialized) {
        return NB_REFLEX_PRIORITY_UNCLAIMED;
    }

    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    nb_reflex_reaction_t reaction;

    nb_event_t event;
    while (nb_event_bus_shell_poll(&event) == NB_EVENT_BUS_OK) {
        if (event.type != NB_EVENT_TYPE_TOUCH || event.payload_len != sizeof(nb_touch_event_payload_t)) {
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
