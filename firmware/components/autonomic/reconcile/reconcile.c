#include "reconcile.h"

#include <string.h>

void nb_reconcile_init(nb_reconcile_state_t *state)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    nb_arc_core_init(&state->arc);
}

void nb_reconcile_tick(nb_reconcile_state_t *state, uint32_t dt_ms, bool valence_negative,
                       bool caress_active)
{
    if (state == NULL) {
        return;
    }

    if (!nb_arc_core_is_running(&state->arc)) {
        if (valence_negative && caress_active) {
            nb_arc_core_start(&state->arc, NB_RECONCILE_ORIENT_MS, NB_RECONCILE_EXECUTE_MS,
                              NB_RECONCILE_OUTCOME_MS, 0u);
        }
        nb_arc_core_tick(&state->arc, dt_ms);
        return;
    }

    /* Rodando: avança primeiro, decide depois com base na fase
     * resultante -- um dt grande que já cascateia até OUTCOME dentro
     * desta mesma chamada conta como "suavização concluída", mesmo que
     * caress_active leia false neste frame (RFC: aborta se o carinho
     * parar ANTES da suavização, não durante/depois dela). */
    const bool was_outcome = (state->arc.phase == NB_ARC_PHASE_OUTCOME);
    nb_arc_core_tick(&state->arc, dt_ms);

    if (state->arc.phase != NB_ARC_PHASE_OUTCOME) {
        if (!caress_active) {
            nb_arc_core_abort(&state->arc);
        }
        return;
    }

    /* Acabou de entrar em OUTCOME neste tick (não estava antes, está
     * agora): tira o snapshot do carinho pro glifo. */
    if (!was_outcome) {
        state->caress_active_at_outcome_entry = caress_active;
    }
}

void nb_reconcile_abort(nb_reconcile_state_t *state)
{
    if (state == NULL) {
        return;
    }
    nb_arc_core_abort(&state->arc);
}

nb_reconcile_beat_t nb_reconcile_current_beat(const nb_reconcile_state_t *state)
{
    if (state == NULL) {
        return NB_RECONCILE_BEAT_NONE;
    }

    switch (state->arc.phase) {
    case NB_ARC_PHASE_ORIENT:
        return NB_RECONCILE_BEAT_GAZE_FRONT;
    case NB_ARC_PHASE_EXECUTE:
        return NB_RECONCILE_BEAT_SMOOTHING;
    case NB_ARC_PHASE_OUTCOME:
        return state->caress_active_at_outcome_entry ? NB_RECONCILE_BEAT_CONTENT_SLOW_BLINK_HEART
                                                      : NB_RECONCILE_BEAT_CONTENT_SLOW_BLINK;
    case NB_ARC_PHASE_IDLE:
    default:
        return NB_RECONCILE_BEAT_NONE;
    }
}
