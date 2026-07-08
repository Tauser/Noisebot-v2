#include "peak_core.h"

#include <string.h>

void nb_peak_core_init(nb_peak_state_t *state)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->active = NB_PEAK_MECHANISM_NONE;
}

bool nb_peak_core_is_eye_glyph(nb_peak_mechanism_t mechanism)
{
    return mechanism == NB_PEAK_MECHANISM_HEART || mechanism == NB_PEAK_MECHANISM_TEARS ||
           mechanism == NB_PEAK_MECHANISM_LAUGH;
}

void nb_peak_core_tick(nb_peak_state_t *state, uint32_t dt_ms, nb_peak_mechanism_t requested)
{
    if (state == NULL) {
        return;
    }
    state->now_ms += dt_ms;

    if (state->active == NB_PEAK_MECHANISM_NONE) {
        if (requested != NB_PEAK_MECHANISM_NONE) {
            state->active = requested;
            state->active_since_ms = state->now_ms;
        }
        return;
    }

    /* ZZZ não expira sozinho -- só some quando deixa de ser pedido (ou
     * por reset_transient). */
    if (state->active == NB_PEAK_MECHANISM_ZZZ) {
        if (requested != NB_PEAK_MECHANISM_ZZZ) {
            state->active = NB_PEAK_MECHANISM_NONE;
        }
        return;
    }

    const uint64_t elapsed = state->now_ms - state->active_since_ms;
    if (elapsed >= NB_PEAK_HOLD_MS) {
        state->active = NB_PEAK_MECHANISM_NONE;
    }
    /* Enquanto dentro do hold, o slot ignora qualquer outro pedido
     * concorrente (exclusividade) -- mesmo um novo pedido do próprio
     * mecanismo ativo não reinicia o timer (pico é breve por definição,
     * não "sustentável" segurando o pedido). */
}

void nb_peak_core_reset_transient(nb_peak_state_t *state)
{
    if (state == NULL) {
        return;
    }
    state->active = NB_PEAK_MECHANISM_NONE;
}

nb_peak_mechanism_t nb_peak_core_active_mechanism(const nb_peak_state_t *state)
{
    return (state == NULL) ? NB_PEAK_MECHANISM_NONE : state->active;
}

float nb_peak_core_fade_scale(const nb_peak_state_t *state)
{
    if (state == NULL || state->active == NB_PEAK_MECHANISM_NONE) {
        return 0.0f;
    }
    if (state->active == NB_PEAK_MECHANISM_ZZZ) {
        return 1.0f;
    }

    const uint64_t elapsed = state->now_ms - state->active_since_ms;
    if (elapsed < NB_PEAK_FADE_MS) {
        return (float)elapsed / (float)NB_PEAK_FADE_MS;
    }
    const uint64_t fade_out_start = NB_PEAK_HOLD_MS - NB_PEAK_FADE_MS;
    if (elapsed >= fade_out_start) {
        const uint64_t into_fade_out = elapsed - fade_out_start;
        if (into_fade_out >= NB_PEAK_FADE_MS) {
            return 0.0f;
        }
        return 1.0f - (float)into_fade_out / (float)NB_PEAK_FADE_MS;
    }
    return 1.0f;
}

bool nb_peak_core_blink_should_pause(const nb_peak_state_t *state)
{
    return state != NULL && nb_peak_core_is_eye_glyph(state->active);
}
