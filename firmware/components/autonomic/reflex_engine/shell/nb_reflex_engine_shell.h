#ifndef NB_REFLEX_ENGINE_SHELL_H
#define NB_REFLEX_ENGINE_SHELL_H

#include "emotion_core.h"
#include "reflex_engine.h"
#include "tiny_fsm.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Casca do reflex_engine (S3.2): dona da instância única do núcleo puro,
 * drena o event_bus (NB_EVENT_TYPE_TOUCH, publicado por
 * touch_service_shell) e aplica a reação vencedora em emotion_core/
 * tiny_fsm. Chamada de dentro da task que já possui os dois (face_demo_task
 * em main.c) -- não roda em task própria. Clock: esp_timer (mesma base
 * absoluta do timestamp publicado no bus), não o dt_ms acumulado do
 * chamador -- evita dois relógios divergentes entre publish e tick.
 */

void nb_reflex_engine_shell_init(void);

/* Aplica a reação vencedora em *emotion (delta afetivo) e *fsm (evento),
 * zera a pilha de claims quando *fsm pousa em IDLE (invariante X->IDLE,
 * ARCHITECTURE.md §4). Retorna a prioridade ativa (P0-P6) pra quem chama
 * decidir se sobrepõe motifs de idle. */
nb_reflex_priority_t nb_reflex_engine_shell_tick(nb_emotion_state_t *emotion, nb_tiny_fsm_t *fsm);

/* S3.5: aplica um estímulo pontual (hoje só NB_REFLEX_STIMULUS_TIMER_FIRED,
 * disparado por schedule_core_shell) reaproveitando on_stimulus/
 * apply_reaction já existentes -- inclui o overlay de toque do
 * led_service (mesma linguagem visual, sem overlay dedicado nesta fatia). */
void nb_reflex_engine_shell_apply_stimulus(nb_reflex_stimulus_t stimulus, nb_emotion_state_t *emotion,
                                           nb_tiny_fsm_t *fsm);

#ifdef __cplusplus
}
#endif

#endif
