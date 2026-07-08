#include "rarity_core.h"

#include <string.h>

void nb_rarity_core_init(nb_rarity_state_t *state)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
}

bool nb_rarity_core_trigger_sneeze(nb_rarity_state_t *state, uint64_t now_ms)
{
    if (state == NULL) {
        return false;
    }
    if (state->has_last_sneeze_trigger &&
        now_ms - state->last_sneeze_trigger_ms < NB_RARITY_SNEEZE_MIN_INTERVAL_MS) {
        return false;
    }
    state->last_sneeze_trigger_ms = now_ms;
    state->has_last_sneeze_trigger = true;
    state->counts[NB_RARITY_SNEEZE]++;
    return true;
}

bool nb_rarity_core_trigger_dream(nb_rarity_state_t *state, bool is_sleeping)
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
    state->dream_available_this_session = false;
    state->counts[NB_RARITY_DREAM]++;
    return true;
}

bool nb_rarity_core_trigger_stargaze(nb_rarity_state_t *state, bool is_night)
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
