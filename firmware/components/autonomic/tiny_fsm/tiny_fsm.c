#include "tiny_fsm.h"

#include <string.h>

static void set_transient_for_state(nb_fsm_transient_t *t, nb_fsm_state_t state)
{
    t->attentive_motif = (state == NB_FSM_STATE_ATTENTIVE);
    t->speaking = (state == NB_FSM_STATE_RESPONDING);
    t->touch_reaction = (state == NB_FSM_STATE_TOUCH_REACTING);
    t->sleeping_visual = (state == NB_FSM_STATE_SLEEPING);
    t->error_icon = (state == NB_FSM_STATE_ERROR);
    t->safe_mode_icon = (state == NB_FSM_STATE_SAFE_MODE);

    if (state == NB_FSM_STATE_IDLE) {
        t->gaze_x = 0.0f;
        t->gaze_y = 0.0f;
    } else if (state == NB_FSM_STATE_ATTENTIVE || state == NB_FSM_STATE_TOUCH_REACTING) {
        t->gaze_x = 0.35f; /* olhar buscando a fonte do estímulo */
        t->gaze_y = 0.0f;
    } else if (state == NB_FSM_STATE_SLEEPING) {
        t->gaze_x = 0.0f;
        t->gaze_y = -0.9f; /* olhos baixos */
    }
    /* BOOT/RESPONDING/ERROR/SAFE_MODE: gaze mantém o valor anterior de
     * propósito -- é o resíduo que o invariante de IDLE precisa zerar
     * mesmo vindo de um estado que não mexe no gaze por si só. */
}

static void transition_to(nb_tiny_fsm_t *fsm, nb_fsm_state_t new_state)
{
    fsm->state = new_state;
    set_transient_for_state(&fsm->transient, new_state);
}

void nb_tiny_fsm_init(nb_tiny_fsm_t *fsm)
{
    if (fsm == NULL) {
        return;
    }
    memset(fsm, 0, sizeof(*fsm));
    fsm->state = NB_FSM_STATE_BOOT;
    fsm->modes = NB_FSM_MODE_NONE;
    set_transient_for_state(&fsm->transient, NB_FSM_STATE_BOOT);
}

bool nb_tiny_fsm_apply_event(nb_tiny_fsm_t *fsm, nb_fsm_event_t event)
{
    if (fsm == NULL) {
        return false;
    }

    switch (event) {
    case NB_FSM_EVENT_MODE_QUIET_ENTER:
        fsm->modes |= (uint32_t)NB_FSM_MODE_QUIET;
        return true;
    case NB_FSM_EVENT_MODE_QUIET_EXIT:
        fsm->modes &= ~(uint32_t)NB_FSM_MODE_QUIET;
        return true;
    case NB_FSM_EVENT_MODE_COMPANION_ENTER:
        fsm->modes |= (uint32_t)NB_FSM_MODE_COMPANION;
        return true;
    case NB_FSM_EVENT_MODE_COMPANION_EXIT:
        fsm->modes &= ~(uint32_t)NB_FSM_MODE_COMPANION;
        return true;
    case NB_FSM_EVENT_MODE_MAINTENANCE_ENTER:
        fsm->modes |= (uint32_t)NB_FSM_MODE_MAINTENANCE;
        return true;
    case NB_FSM_EVENT_MODE_MAINTENANCE_EXIT:
        fsm->modes &= ~(uint32_t)NB_FSM_MODE_MAINTENANCE;
        return true;
    default:
        break;
    }

    /* Safety vence tudo (BEHAVIOR.md §1.2), exceto SAFE_MODE: pegajoso,
     * só sai por reset externo (fora desta API -- ver CLAUDE.md motion
     * safety, mesma regra generalizada aqui). */
    if (fsm->state != NB_FSM_STATE_SAFE_MODE) {
        if (event == NB_FSM_EVENT_FAULT_CRITICAL) {
            transition_to(fsm, NB_FSM_STATE_SAFE_MODE);
            return true;
        }
        if (event == NB_FSM_EVENT_FAULT) {
            transition_to(fsm, NB_FSM_STATE_ERROR);
            return true;
        }
    }

    switch (fsm->state) {
    case NB_FSM_STATE_BOOT:
        if (event == NB_FSM_EVENT_BOOT_OK) {
            transition_to(fsm, NB_FSM_STATE_IDLE);
            return true;
        }
        if (event == NB_FSM_EVENT_BOOT_FAIL_CRITICAL) {
            transition_to(fsm, NB_FSM_STATE_SAFE_MODE);
            return true;
        }
        break;

    case NB_FSM_STATE_IDLE:
        if (event == NB_FSM_EVENT_WAKE) {
            transition_to(fsm, NB_FSM_STATE_ATTENTIVE);
            return true;
        }
        if (event == NB_FSM_EVENT_SAY_START) {
            transition_to(fsm, NB_FSM_STATE_RESPONDING);
            return true;
        }
        if (event == NB_FSM_EVENT_TOUCH) {
            transition_to(fsm, NB_FSM_STATE_TOUCH_REACTING);
            return true;
        }
        if (event == NB_FSM_EVENT_SLEEP) {
            transition_to(fsm, NB_FSM_STATE_SLEEPING);
            return true;
        }
        break;

    case NB_FSM_STATE_ATTENTIVE:
        if (event == NB_FSM_EVENT_SAY_START) {
            transition_to(fsm, NB_FSM_STATE_RESPONDING);
            return true;
        }
        if (event == NB_FSM_EVENT_ATTENTIVE_TIMEOUT) {
            transition_to(fsm, NB_FSM_STATE_IDLE);
            return true;
        }
        if (event == NB_FSM_EVENT_TOUCH) {
            transition_to(fsm, NB_FSM_STATE_TOUCH_REACTING);
            return true;
        }
        break;

    case NB_FSM_STATE_RESPONDING:
        if (event == NB_FSM_EVENT_SAY_END) {
            transition_to(fsm, NB_FSM_STATE_IDLE);
            return true;
        }
        if (event == NB_FSM_EVENT_SAY_FOLLOWUP) {
            transition_to(fsm, NB_FSM_STATE_ATTENTIVE);
            return true;
        }
        if (event == NB_FSM_EVENT_TOUCH) {
            /* touch vence fala: barge-in físico, cancela o turno */
            transition_to(fsm, NB_FSM_STATE_TOUCH_REACTING);
            return true;
        }
        if (event == NB_FSM_EVENT_SERVER_DROPPED) {
            transition_to(fsm, NB_FSM_STATE_IDLE);
            return true;
        }
        break;

    case NB_FSM_STATE_TOUCH_REACTING:
        if (event == NB_FSM_EVENT_TOUCH_END) {
            transition_to(fsm, NB_FSM_STATE_IDLE);
            return true;
        }
        break;

    case NB_FSM_STATE_SLEEPING:
        if (event == NB_FSM_EVENT_TOUCH || event == NB_FSM_EVENT_WAKE_HOUR) {
            transition_to(fsm, NB_FSM_STATE_IDLE);
            return true;
        }
        if (event == NB_FSM_EVENT_WAKE) {
            if ((fsm->modes & (uint32_t)NB_FSM_MODE_QUIET) != 0u) {
                return false; /* wake ignorado em modo quiet */
            }
            transition_to(fsm, NB_FSM_STATE_ATTENTIVE);
            return true;
        }
        break;

    case NB_FSM_STATE_ERROR:
        if (event == NB_FSM_EVENT_RECOVER) {
            transition_to(fsm, NB_FSM_STATE_IDLE);
            return true;
        }
        break;

    case NB_FSM_STATE_SAFE_MODE:
        break; /* pegajoso -- nenhum evento desta API tira daqui */

    default:
        break;
    }

    return false;
}

nb_fsm_state_t nb_tiny_fsm_get_state(const nb_tiny_fsm_t *fsm)
{
    return (fsm != NULL) ? fsm->state : NB_FSM_STATE_BOOT;
}

uint32_t nb_tiny_fsm_get_modes(const nb_tiny_fsm_t *fsm)
{
    return (fsm != NULL) ? fsm->modes : (uint32_t)NB_FSM_MODE_NONE;
}

const nb_fsm_transient_t *nb_tiny_fsm_get_transient(const nb_tiny_fsm_t *fsm)
{
    return (fsm != NULL) ? &fsm->transient : NULL;
}

const char *nb_tiny_fsm_state_name(nb_fsm_state_t state)
{
    static const char *const names[NB_FSM_STATE_COUNT] = {
        "BOOT", "IDLE", "ATTENTIVE", "RESPONDING",
        "TOUCH_REACTING", "SLEEPING", "ERROR", "SAFE_MODE",
    };

    if ((uint32_t)state >= (uint32_t)NB_FSM_STATE_COUNT) {
        return "UNKNOWN";
    }
    return names[state];
}
