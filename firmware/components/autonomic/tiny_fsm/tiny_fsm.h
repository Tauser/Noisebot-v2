#ifndef NB_TINY_FSM_H
#define NB_TINY_FSM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro da máquina de estados de alto nível (S2.3, BEHAVIOR.md
 * §1). Sem FreeRTOS/ESP-IDF/event_bus -- eventos chegam por chamada direta
 * de nb_tiny_fsm_apply_event(), a casca (quando um consumidor real existir)
 * traduz eventos do event bus pra esta API, igual ao event_bus (S1.2).
 *
 * Invariante central (ARCHITECTURE.md §4, BEHAVIOR.md §1.1, gate H7): toda
 * transição X→IDLE limpa o estado transitório antes de aceitar qualquer
 * coisa nova. `nb_tiny_fsm_transient_t` é esse estado transitório —
 * expressão/gaze/boca/overlay/LED que os estados não-IDLE ligam e que
 * SEMPRE volta ao valor canônico limpo ao pousar em IDLE, não importa de
 * qual estado veio nem quais modos estavam ativos.
 */

typedef enum {
    NB_FSM_STATE_BOOT = 0,
    NB_FSM_STATE_IDLE = 1,
    NB_FSM_STATE_ATTENTIVE = 2,
    NB_FSM_STATE_RESPONDING = 3,
    NB_FSM_STATE_TOUCH_REACTING = 4,
    NB_FSM_STATE_SLEEPING = 5,
    NB_FSM_STATE_ERROR = 6,
    NB_FSM_STATE_SAFE_MODE = 7,
    NB_FSM_STATE_COUNT = 8,
} nb_fsm_state_t;

typedef enum {
    NB_FSM_EVENT_BOOT_OK = 0,
    NB_FSM_EVENT_BOOT_FAIL_CRITICAL,
    NB_FSM_EVENT_WAKE,             /* wake word, presença, som saliente */
    NB_FSM_EVENT_SAY_START,        /* turno de conversa (SAY_*) começa */
    NB_FSM_EVENT_SAY_END,          /* turno termina ou é cancelado */
    NB_FSM_EVENT_SAY_FOLLOWUP,     /* turno vira follow-up (mantém atenção) */
    NB_FSM_EVENT_ATTENTIVE_TIMEOUT, /* 8-15s sem turno em ATTENTIVE */
    NB_FSM_EVENT_TOUCH,            /* TAP/LONG/SUSTAINED */
    NB_FSM_EVENT_TOUCH_END,        /* fim da reação de toque */
    NB_FSM_EVENT_SLEEP,            /* circadiano/inatividade/comando */
    NB_FSM_EVENT_WAKE_HOUR,        /* horário de acordar programado */
    NB_FSM_EVENT_SERVER_DROPPED,   /* mente caiu no meio de RESPONDING */
    NB_FSM_EVENT_FAULT,            /* falha recuperável -- vence tudo */
    NB_FSM_EVENT_FAULT_CRITICAL,   /* falha de boot/safety -- pegajosa */
    NB_FSM_EVENT_RECOVER,          /* saída de ERROR */
    NB_FSM_EVENT_MODE_QUIET_ENTER,
    NB_FSM_EVENT_MODE_QUIET_EXIT,
    NB_FSM_EVENT_MODE_COMPANION_ENTER,
    NB_FSM_EVENT_MODE_COMPANION_EXIT,
    NB_FSM_EVENT_MODE_MAINTENANCE_ENTER,
    NB_FSM_EVENT_MODE_MAINTENANCE_EXIT,
    NB_FSM_EVENT_COUNT,
} nb_fsm_event_t;

/* Modos são flags persistentes, não estados -- sobrevivem a troca de
 * estado (BEHAVIOR.md §1: "Modos de IDLE"; a exceção de wake em modo
 * quiet durante SLEEPING em §1.2 só faz sentido se o modo persistir fora
 * de IDLE). */
typedef enum {
    NB_FSM_MODE_NONE = 0,
    NB_FSM_MODE_QUIET = 1u << 0,
    NB_FSM_MODE_COMPANION = 1u << 1,
    NB_FSM_MODE_MAINTENANCE = 1u << 2,
} nb_fsm_mode_t;

/* Estado transitório que os estados não-IDLE ligam. Zerado por completo
 * (todos os bools false, gaze em 0,0) toda vez que a máquina pousa em
 * IDLE -- é isso que o host-test de invariante verifica. */
typedef struct {
    bool attentive_motif; /* motifs de atenção em ATTENTIVE */
    bool speaking;        /* boca ativa em RESPONDING */
    bool touch_reaction;  /* reação afetiva em TOUCH_REACTING */
    bool sleeping_visual; /* olhos fechados/respiração em SLEEPING */
    bool error_icon;      /* expressão ALARMED + ícone em ERROR */
    bool safe_mode_icon;  /* face mínima + ícone em SAFE_MODE */
    float gaze_x;
    float gaze_y;
} nb_fsm_transient_t;

typedef struct {
    nb_fsm_state_t state;
    uint32_t modes; /* OR de nb_fsm_mode_t */
    nb_fsm_transient_t transient;
} nb_tiny_fsm_t;

/* Estado inicial: BOOT, sem modos, transiente zerado. */
void nb_tiny_fsm_init(nb_tiny_fsm_t *fsm);

/* Aplica um evento. Retorna true se causou transição de estado (mudança
 * de modo sem troca de estado também retorna true). Combinação inválida
 * de (estado atual, evento) é no-op silencioso -- retorna false. */
bool nb_tiny_fsm_apply_event(nb_tiny_fsm_t *fsm, nb_fsm_event_t event);

nb_fsm_state_t nb_tiny_fsm_get_state(const nb_tiny_fsm_t *fsm);
uint32_t nb_tiny_fsm_get_modes(const nb_tiny_fsm_t *fsm);
const nb_fsm_transient_t *nb_tiny_fsm_get_transient(const nb_tiny_fsm_t *fsm);

/* Nome estático do estado, para log. Índice fora do intervalo retorna
 * "UNKNOWN". */
const char *nb_tiny_fsm_state_name(nb_fsm_state_t state);

#ifdef __cplusplus
}
#endif

#endif
