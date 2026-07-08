#include "arc_core.h"

#include <string.h>

void nb_arc_core_init(nb_arc_state_t *arc)
{
    if (arc == NULL) {
        return;
    }
    memset(arc, 0, sizeof(*arc));
    arc->phase = NB_ARC_PHASE_IDLE;
}

bool nb_arc_core_is_running(const nb_arc_state_t *arc)
{
    return arc != NULL && arc->phase != NB_ARC_PHASE_IDLE;
}

bool nb_arc_core_in_cooldown(const nb_arc_state_t *arc)
{
    if (arc == NULL || arc->phase != NB_ARC_PHASE_IDLE) {
        return false;
    }
    return arc->now_ms < arc->cooldown_until_ms;
}

bool nb_arc_core_start(nb_arc_state_t *arc, uint32_t orient_ms, uint32_t execute_ms,
                       uint32_t outcome_ms, uint32_t cooldown_ms)
{
    if (arc == NULL) {
        return false;
    }
    if (nb_arc_core_is_running(arc) || nb_arc_core_in_cooldown(arc)) {
        return false;
    }

    arc->phase = NB_ARC_PHASE_ORIENT;
    arc->phase_started_ms = arc->now_ms;
    arc->orient_ms = orient_ms;
    arc->execute_ms = execute_ms;
    arc->outcome_ms = outcome_ms;
    arc->cooldown_ms = cooldown_ms;
    return true;
}

/* Cascateia quantas transições de fase couberem no dt (dt grande pode
 * atravessar mais de uma fase numa só chamada -- o resto de tempo não
 * consumido por uma fase carrega pra próxima, phase_started_ms avança
 * pela duração exata, não pula pra now_ms). */
void nb_arc_core_tick(nb_arc_state_t *arc, uint32_t dt_ms)
{
    if (arc == NULL) {
        return;
    }
    arc->now_ms += dt_ms;

    for (;;) {
        if (arc->phase == NB_ARC_PHASE_IDLE) {
            return;
        }

        uint32_t duration = 0u;
        switch (arc->phase) {
        case NB_ARC_PHASE_ORIENT:
            duration = arc->orient_ms;
            break;
        case NB_ARC_PHASE_EXECUTE:
            duration = arc->execute_ms;
            break;
        case NB_ARC_PHASE_OUTCOME:
            duration = arc->outcome_ms;
            break;
        default:
            return;
        }

        const uint64_t elapsed = arc->now_ms - arc->phase_started_ms;
        if (elapsed < duration) {
            return;
        }

        arc->phase_started_ms += duration;
        switch (arc->phase) {
        case NB_ARC_PHASE_ORIENT:
            arc->phase = NB_ARC_PHASE_EXECUTE;
            break;
        case NB_ARC_PHASE_EXECUTE:
            arc->phase = NB_ARC_PHASE_OUTCOME;
            break;
        case NB_ARC_PHASE_OUTCOME:
            arc->phase = NB_ARC_PHASE_IDLE;
            arc->cooldown_until_ms = arc->now_ms + arc->cooldown_ms;
            break;
        default:
            return;
        }
    }
}

void nb_arc_core_abort(nb_arc_state_t *arc)
{
    if (arc == NULL) {
        return;
    }
    arc->phase = NB_ARC_PHASE_IDLE;
}
