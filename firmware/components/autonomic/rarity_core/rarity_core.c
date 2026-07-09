#include "rarity_core.h"

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

/* Poisson memoryless: p = dt_ms * taxa (aproximação padrão pra p pequeno,
 * verdadeira pra dt_ms*taxa << 1 -- vale aqui, taxas são de horas). */
static bool roll(uint32_t *rng_state, uint32_t dt_ms, float rate_per_ms)
{
    const float p = (float)dt_ms * rate_per_ms;
    return rand01(rng_state) < p;
}

void nb_rarity_core_init(nb_rarity_state_t *state, uint32_t rng_seed)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->rng_state = (rng_seed != 0u) ? rng_seed : 0x9E3779B9u;
}

bool nb_rarity_core_tick_sneeze(nb_rarity_state_t *state, uint32_t dt_ms, bool is_eligible,
                                uint64_t now_ms)
{
    if (state == NULL) {
        return false;
    }
    if (!is_eligible) {
        return false;
    }
    if (state->has_last_sneeze_trigger &&
        now_ms - state->last_sneeze_trigger_ms < NB_RARITY_SNEEZE_MIN_INTERVAL_MS) {
        return false;
    }
    if (!roll(&state->rng_state, dt_ms, NB_RARITY_SNEEZE_RATE_PER_MS)) {
        return false;
    }
    state->last_sneeze_trigger_ms = now_ms;
    state->has_last_sneeze_trigger = true;
    state->counts[NB_RARITY_SNEEZE]++;
    return true;
}

bool nb_rarity_core_tick_dream(nb_rarity_state_t *state, uint32_t dt_ms, bool is_sleeping)
{
    if (state == NULL) {
        return false;
    }
    if (!is_sleeping) {
        state->was_sleeping = false;
        state->dream_available_this_session = true; /* rearma pra próxima sessão */
        return false;
    }
    if (!state->was_sleeping) {
        state->dream_available_this_session = true; /* entrou numa sessão nova */
    }
    state->was_sleeping = true;

    if (!state->dream_available_this_session) {
        return false;
    }
    if (!roll(&state->rng_state, dt_ms, NB_RARITY_DREAM_RATE_PER_MS)) {
        return false;
    }
    state->dream_available_this_session = false;
    state->counts[NB_RARITY_DREAM]++;
    return true;
}

bool nb_rarity_core_tick_stargaze(nb_rarity_state_t *state, uint32_t dt_ms, bool is_night)
{
    if (state == NULL) {
        return false;
    }
    if (!is_night) {
        state->was_night = false;
        state->stargaze_available_this_session = true;
        return false;
    }
    if (!state->was_night) {
        state->stargaze_available_this_session = true;
    }
    state->was_night = true;

    if (!state->stargaze_available_this_session) {
        return false;
    }
    if (!roll(&state->rng_state, dt_ms, NB_RARITY_STARGAZE_RATE_PER_MS)) {
        return false;
    }
    state->stargaze_available_this_session = false;
    state->counts[NB_RARITY_STARGAZE]++;
    return true;
}

uint16_t nb_rarity_core_count(const nb_rarity_state_t *state, nb_rarity_kind_t kind)
{
    if (state == NULL || kind >= NB_RARITY_COUNT) {
        return 0u;
    }
    return state->counts[kind];
}
