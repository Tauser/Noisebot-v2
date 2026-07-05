#include "reflex_engine.h"

#include <string.h>

/* Limiares de reclassificação de toque contínuo (BEHAVIOR.md §2/§4). TAP e
 * LONG_PRESS chegam discretos do touch_service; daqui em diante é duração
 * real medida por nb_touch_service_get_duration_ms(). */
#define NB_REFLEX_TOUCH_WARM_PULSE_MS 3000u
#define NB_REFLEX_TOUCH_DEEP_MS 8000u
#define NB_REFLEX_TOUCH_CARESS_MS 15000u
#define NB_REFLEX_WARM_PULSE_PERIOD_MS 4000u

typedef struct {
    nb_reflex_priority_t band; /* NB_REFLEX_PRIORITY_UNCLAIMED = sem claim, só afeto */
    float delta_valence;
    float delta_arousal;
    nb_fsm_event_t fsm_event; /* NB_FSM_EVENT_COUNT = nenhum */
    uint32_t claim_duration_ms; /* 0 = não abre claim */
} nb_reflex_table_entry_t;

/* Valores exatos são #define/literais de tuning, ajustados em bancada e
 * travados por host-test de regressão (mesma nota de emotion_core) --
 * sinais e magnitudes relativas seguem a tabela de BEHAVIOR.md §2. */
static const nb_reflex_table_entry_t NB_REFLEX_TABLE[NB_REFLEX_STIMULUS_COUNT] = {
    [NB_REFLEX_STIMULUS_TOUCH_TAP] = {
        NB_REFLEX_PRIORITY_TOUCH, 0.20f, 0.15f, NB_FSM_EVENT_TOUCH, 400u,
    },
    [NB_REFLEX_STIMULUS_TOUCH_LONG] = {
        NB_REFLEX_PRIORITY_TOUCH, 0.45f, 0.20f, NB_FSM_EVENT_TOUCH, 3000u,
    },
    [NB_REFLEX_STIMULUS_TOUCH_WARM_PULSE] = {
        NB_REFLEX_PRIORITY_TOUCH, 0.45f, 0.00f, NB_FSM_EVENT_TOUCH, NB_REFLEX_WARM_PULSE_PERIOD_MS,
    },
    [NB_REFLEX_STIMULUS_TOUCH_DEEP] = {
        NB_REFLEX_PRIORITY_TOUCH, 0.65f, -0.20f, NB_FSM_EVENT_TOUCH, NB_REFLEX_WARM_PULSE_PERIOD_MS,
    },
    [NB_REFLEX_STIMULUS_TOUCH_CARESS] = {
        NB_REFLEX_PRIORITY_TOUCH, 0.85f, -0.35f, NB_FSM_EVENT_TOUCH, NB_REFLEX_WARM_PULSE_PERIOD_MS,
    },
    [NB_REFLEX_STIMULUS_TOUCH_RELEASE] = {
        NB_REFLEX_PRIORITY_TOUCH, 0.00f, 0.00f, NB_FSM_EVENT_TOUCH_END, 300u,
    },
    [NB_REFLEX_STIMULUS_TOUCH_WAKE] = {
        NB_REFLEX_PRIORITY_TOUCH, 0.20f, 0.30f, NB_FSM_EVENT_WAKE, 400u,
    },
    [NB_REFLEX_STIMULUS_VOICE_START] = {
        NB_REFLEX_PRIORITY_UNCLAIMED, 0.10f, 0.40f, NB_FSM_EVENT_WAKE, 0u,
    },
    [NB_REFLEX_STIMULUS_VOICE_LOUD] = {
        NB_REFLEX_PRIORITY_UNCLAIMED, -0.20f, 0.60f, NB_FSM_EVENT_COUNT, 0u,
    },
    [NB_REFLEX_STIMULUS_VOICE_SOFT] = {
        NB_REFLEX_PRIORITY_UNCLAIMED, 0.15f, 0.15f, NB_FSM_EVENT_COUNT, 0u,
    },
    [NB_REFLEX_STIMULUS_AUDIO_PLAYING] = {
        NB_REFLEX_PRIORITY_UNCLAIMED, 0.10f, 0.15f, NB_FSM_EVENT_COUNT, 0u,
    },
    [NB_REFLEX_STIMULUS_ENTERING_SLEEP] = {
        NB_REFLEX_PRIORITY_UNCLAIMED, 0.00f, -0.60f, NB_FSM_EVENT_SLEEP, 0u,
    },
    [NB_REFLEX_STIMULUS_WAKING_UP] = {
        NB_REFLEX_PRIORITY_UNCLAIMED, 0.15f, 0.20f, NB_FSM_EVENT_WAKE_HOUR, 0u,
    },
    [NB_REFLEX_STIMULUS_MOTION_FAULT] = {
        NB_REFLEX_PRIORITY_SAFETY, -0.65f, 0.60f, NB_FSM_EVENT_FAULT, 5000u,
    },
    [NB_REFLEX_STIMULUS_IDLE_LONG] = {
        NB_REFLEX_PRIORITY_UNCLAIMED, -0.10f, -0.10f, NB_FSM_EVENT_COUNT, 0u,
    },
};

void nb_reflex_engine_init(nb_reflex_engine_t *engine) {
    if (!engine) {
        return;
    }
    memset(engine, 0, sizeof(*engine));
    for (uint32_t band = 0; band < NB_REFLEX_CLAIM_BANDS; band++) {
        engine->claims[band].fsm_event = NB_FSM_EVENT_COUNT;
    }
    engine->touch_current_phase = NB_REFLEX_STIMULUS_COUNT;
}

nb_reflex_priority_t nb_reflex_engine_get_active_priority(const nb_reflex_engine_t *engine,
                                                          uint32_t now_ms) {
    if (!engine) {
        return NB_REFLEX_PRIORITY_UNCLAIMED;
    }
    for (uint32_t band = 0; band < NB_REFLEX_CLAIM_BANDS; band++) {
        const nb_reflex_claim_t *claim = &engine->claims[band];
        if (claim->active && now_ms < claim->expires_at_ms) {
            return (nb_reflex_priority_t)band;
        }
    }
    return NB_REFLEX_PRIORITY_UNCLAIMED;
}

static void fill_reaction(const nb_reflex_engine_t *engine, const nb_reflex_table_entry_t *entry,
                          uint32_t now_ms, nb_reflex_reaction_t *out_reaction) {
    if (!out_reaction) {
        return;
    }
    out_reaction->active_priority = nb_reflex_engine_get_active_priority(engine, now_ms);
    out_reaction->fsm_event = entry->fsm_event;
    out_reaction->has_affect_delta = (entry->delta_valence != 0.0f) || (entry->delta_arousal != 0.0f);
    out_reaction->delta_valence = entry->delta_valence;
    out_reaction->delta_arousal = entry->delta_arousal;
}

void nb_reflex_engine_on_stimulus(nb_reflex_engine_t *engine, nb_reflex_stimulus_t stimulus,
                                  uint32_t now_ms, nb_reflex_reaction_t *out_reaction) {
    if (!engine || stimulus >= NB_REFLEX_STIMULUS_COUNT) {
        if (out_reaction) {
            memset(out_reaction, 0, sizeof(*out_reaction));
            out_reaction->active_priority = NB_REFLEX_PRIORITY_UNCLAIMED;
            out_reaction->fsm_event = NB_FSM_EVENT_COUNT;
        }
        return;
    }

    const nb_reflex_table_entry_t *entry = &NB_REFLEX_TABLE[stimulus];
    engine->now_ms = now_ms;

    if (entry->band != NB_REFLEX_PRIORITY_UNCLAIMED && entry->claim_duration_ms > 0u) {
        nb_reflex_claim_t *claim = &engine->claims[entry->band];
        claim->active = true;
        claim->activated_at_ms = now_ms;
        claim->expires_at_ms = now_ms + entry->claim_duration_ms;
        claim->fsm_event = entry->fsm_event;
    }

    fill_reaction(engine, entry, now_ms, out_reaction);
}

void nb_reflex_engine_tick(nb_reflex_engine_t *engine, uint32_t dt_ms, bool touch_is_pressed,
                           uint32_t touch_duration_ms, nb_reflex_reaction_t *out_reaction) {
    if (!engine) {
        return;
    }
    uint32_t now_ms = engine->now_ms + dt_ms;
    engine->now_ms = now_ms;

    if (out_reaction) {
        memset(out_reaction, 0, sizeof(*out_reaction));
        out_reaction->fsm_event = NB_FSM_EVENT_COUNT;
        out_reaction->active_priority = nb_reflex_engine_get_active_priority(engine, now_ms);
    }

    if (touch_is_pressed) {
        nb_reflex_stimulus_t phase = NB_REFLEX_STIMULUS_COUNT;
        if (touch_duration_ms >= NB_REFLEX_TOUCH_CARESS_MS) {
            phase = NB_REFLEX_STIMULUS_TOUCH_CARESS;
        } else if (touch_duration_ms >= NB_REFLEX_TOUCH_DEEP_MS) {
            phase = NB_REFLEX_STIMULUS_TOUCH_DEEP;
        } else if (touch_duration_ms >= NB_REFLEX_TOUCH_WARM_PULSE_MS) {
            phase = NB_REFLEX_STIMULUS_TOUCH_WARM_PULSE;
        }

        if (phase != NB_REFLEX_STIMULUS_COUNT) {
            bool phase_changed = (phase != engine->touch_current_phase);
            bool pulse_due = (phase == NB_REFLEX_STIMULUS_TOUCH_WARM_PULSE) &&
                              (now_ms - engine->last_touch_phase_stimulus_at_ms >=
                               NB_REFLEX_WARM_PULSE_PERIOD_MS);
            if (phase_changed || pulse_due) {
                nb_reflex_engine_on_stimulus(engine, phase, now_ms, out_reaction);
                engine->last_touch_phase_stimulus_at_ms = now_ms;
            }
            engine->touch_current_phase = phase;
        }
        engine->touch_was_pressed = true;
    } else {
        if (engine->touch_was_pressed) {
            nb_reflex_engine_on_stimulus(engine, NB_REFLEX_STIMULUS_TOUCH_RELEASE, now_ms, out_reaction);
        }
        engine->touch_was_pressed = false;
        engine->touch_current_phase = NB_REFLEX_STIMULUS_COUNT;
        engine->last_touch_phase_stimulus_at_ms = 0u;
    }

    for (uint32_t band = 0; band < NB_REFLEX_CLAIM_BANDS; band++) {
        nb_reflex_claim_t *claim = &engine->claims[band];
        if (claim->active && now_ms >= claim->expires_at_ms) {
            claim->active = false;
        }
    }

    if (out_reaction) {
        out_reaction->active_priority = nb_reflex_engine_get_active_priority(engine, now_ms);
    }
}

void nb_reflex_engine_force_clear(nb_reflex_engine_t *engine) {
    if (!engine) {
        return;
    }
    for (uint32_t band = 0; band < NB_REFLEX_CLAIM_BANDS; band++) {
        engine->claims[band].active = false;
        engine->claims[band].expires_at_ms = 0u;
        engine->claims[band].fsm_event = NB_FSM_EVENT_COUNT;
    }
    engine->touch_current_phase = NB_REFLEX_STIMULUS_COUNT;
    engine->touch_was_pressed = false;
    engine->last_touch_phase_stimulus_at_ms = 0u;
}
