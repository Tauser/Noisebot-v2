#include "grumpy_forgive.h"

#include <string.h>

void nb_grumpy_forgive_init(nb_grumpy_forgive_state_t *state)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    nb_arc_core_init(&state->arc);
}

bool nb_grumpy_forgive_on_tap(nb_grumpy_forgive_state_t *state, uint64_t now_ms)
{
    if (state == NULL) {
        return false;
    }

    state->tap_times_ms[state->tap_next_slot] = now_ms;
    state->tap_next_slot = (state->tap_next_slot + 1u) % NB_GRUMPY_FORGIVE_TAP_THRESHOLD;
    if (state->tap_count < NB_GRUMPY_FORGIVE_TAP_THRESHOLD) {
        ++state->tap_count;
    }

    if (state->tap_count < NB_GRUMPY_FORGIVE_TAP_THRESHOLD) {
        return false;
    }

    /* Anel cheio: tap_next_slot já aponta pro mais antigo dos últimos
     * NB_GRUMPY_FORGIVE_TAP_THRESHOLD taps (é o próximo a ser
     * sobrescrito). */
    const uint64_t oldest = state->tap_times_ms[state->tap_next_slot];
    if (now_ms - oldest > NB_GRUMPY_FORGIVE_TAP_WINDOW_MS) {
        return false;
    }

    return nb_arc_core_start(&state->arc, 0u, NB_GRUMPY_FORGIVE_EXECUTE_MS,
                             NB_GRUMPY_FORGIVE_OUTCOME_MS, NB_GRUMPY_FORGIVE_COOLDOWN_MS);
}

void nb_grumpy_forgive_tick(nb_grumpy_forgive_state_t *state, uint32_t dt_ms)
{
    if (state == NULL) {
        return;
    }
    nb_arc_core_tick(&state->arc, dt_ms);
}

void nb_grumpy_forgive_abort(nb_grumpy_forgive_state_t *state)
{
    if (state == NULL) {
        return;
    }
    nb_arc_core_abort(&state->arc);
}

nb_grumpy_forgive_beat_t nb_grumpy_forgive_current_beat(const nb_grumpy_forgive_state_t *state)
{
    if (state == NULL) {
        return NB_GRUMPY_FORGIVE_BEAT_NONE;
    }

    switch (state->arc.phase) {
    case NB_ARC_PHASE_EXECUTE:
        return NB_GRUMPY_FORGIVE_BEAT_ANGRY;
    case NB_ARC_PHASE_OUTCOME: {
        const uint64_t elapsed = state->arc.now_ms - state->arc.phase_started_ms;
        return (elapsed < NB_GRUMPY_FORGIVE_GAZE_MS) ? NB_GRUMPY_FORGIVE_BEAT_BLINK_GAZE_AWAY
                                                     : NB_GRUMPY_FORGIVE_BEAT_NEUTRAL_HAPPY;
    }
    case NB_ARC_PHASE_ORIENT:
    case NB_ARC_PHASE_IDLE:
    default:
        return NB_GRUMPY_FORGIVE_BEAT_NONE;
    }
}
