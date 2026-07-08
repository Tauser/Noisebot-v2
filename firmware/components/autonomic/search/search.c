#include "search.h"

#include <string.h>

static uint32_t xorshift32(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/* [0, 1) com 24 bits de precisão -- mesma técnica de emotion_core/idle_engine. */
static float rand01(uint32_t *state)
{
    return (float)(xorshift32(state) >> 8) / (float)(1u << 24);
}

void nb_search_init(nb_search_state_t *state, uint32_t rng_seed)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    nb_arc_core_init(&state->arc);
    state->rng_state = (rng_seed != 0u) ? rng_seed : 0x9E3779B9u;
    state->last_pattern = NB_SEARCH_PATTERN_COUNT;
    state->has_last_pattern = false;
}

static bool start_search(nb_search_state_t *state)
{
    if (nb_arc_core_is_running(&state->arc) || nb_arc_core_in_cooldown(&state->arc)) {
        return false;
    }

    nb_search_pattern_t pattern;
    do {
        pattern = (nb_search_pattern_t)(xorshift32(&state->rng_state) % NB_SEARCH_PATTERN_COUNT);
    } while (state->has_last_pattern && pattern == state->last_pattern);
    state->last_pattern = pattern;
    state->has_last_pattern = true;

    const uint32_t glance_span = NB_SEARCH_GLANCE_MAX_COUNT - NB_SEARCH_GLANCE_MIN_COUNT + 1u;
    const uint32_t glance_count = NB_SEARCH_GLANCE_MIN_COUNT + (xorshift32(&state->rng_state) % glance_span);
    state->last_glance_count = glance_count;
    const uint32_t execute_ms = glance_count * NB_SEARCH_GLANCE_MS;

    state->outcome_found = rand01(&state->rng_state) < NB_SEARCH_FOUND_PROBABILITY;
    const uint32_t outcome_ms = state->outcome_found
                                     ? NB_SEARCH_OUTCOME_FOUND_MS
                                     : (NB_SEARCH_NOT_FOUND_BLINK_MS + NB_SEARCH_NOT_FOUND_SIGH_MS);

    return nb_arc_core_start(&state->arc, NB_SEARCH_ORIENT_MS, execute_ms, outcome_ms, 0u);
}

bool nb_search_trigger_stimulus(nb_search_state_t *state)
{
    if (state == NULL) {
        return false;
    }
    return start_search(state);
}

bool nb_search_trigger_boredom(nb_search_state_t *state, uint64_t now_ms)
{
    if (state == NULL) {
        return false;
    }
    if (state->has_last_boredom_trigger &&
        now_ms - state->last_boredom_trigger_ms < NB_SEARCH_BOREDOM_MIN_INTERVAL_MS) {
        return false;
    }
    if (!start_search(state)) {
        return false;
    }
    state->last_boredom_trigger_ms = now_ms;
    state->has_last_boredom_trigger = true;
    return true;
}

void nb_search_tick(nb_search_state_t *state, uint32_t dt_ms)
{
    if (state == NULL) {
        return;
    }
    nb_arc_core_tick(&state->arc, dt_ms);
}

void nb_search_abort(nb_search_state_t *state)
{
    if (state == NULL) {
        return;
    }
    nb_arc_core_abort(&state->arc);
}

nb_search_beat_t nb_search_current_beat(const nb_search_state_t *state)
{
    if (state == NULL) {
        return NB_SEARCH_BEAT_NONE;
    }

    switch (state->arc.phase) {
    case NB_ARC_PHASE_ORIENT:
        return NB_SEARCH_BEAT_ORIENT;
    case NB_ARC_PHASE_EXECUTE:
        return NB_SEARCH_BEAT_SEARCHING;
    case NB_ARC_PHASE_OUTCOME:
        if (state->outcome_found) {
            return NB_SEARCH_BEAT_FOUND;
        }
        {
            const uint64_t elapsed = state->arc.now_ms - state->arc.phase_started_ms;
            return (elapsed < NB_SEARCH_NOT_FOUND_BLINK_MS) ? NB_SEARCH_BEAT_NOT_FOUND_BLINK
                                                             : NB_SEARCH_BEAT_NOT_FOUND_SIGH;
        }
    case NB_ARC_PHASE_IDLE:
    default:
        return NB_SEARCH_BEAT_NONE;
    }
}

nb_search_pattern_t nb_search_current_pattern(const nb_search_state_t *state)
{
    if (state == NULL || !state->has_last_pattern) {
        return NB_SEARCH_PATTERN_COUNT;
    }
    return state->last_pattern;
}

uint32_t nb_search_current_glance_count(const nb_search_state_t *state)
{
    return (state == NULL) ? 0u : state->last_glance_count;
}
