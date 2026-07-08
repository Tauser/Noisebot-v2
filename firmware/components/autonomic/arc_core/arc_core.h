#ifndef NB_ARC_CORE_H
#define NB_ARC_CORE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro de sequência genérica de arco (S3.8, item 3,
 * RFC-VIDA-V2.md §5.2 "gramática geral": orientação -> execução ->
 * desfecho; nada termina simplesmente parando; entrada em IDLE aborta
 * limpo em qualquer ato).
 *
 * "Zero âncora nova" (RFC §5.2): este núcleo não sabe nada de
 * emotion_core/idle_engine/touch_service -- só conta fases e tempo. A
 * casca (ou o núcleo específico de cada arco, ex. GRUMPY_FORGIVE) decide
 * o que cada fase dispara nos núcleos vizinhos, mesma separação L4
 * horizontal de emotion_core/idle_engine (ARCHITECTURE.md §2).
 *
 * Um `nb_arc_state_t` conduz um arco por vez. Cooldown é medido a partir
 * do fim do desfecho (transição OUTCOME->IDLE natural) -- abortar não
 * aplica cooldown (não foi "executado", H7 permanece limpo pra retentar).
 *
 * Sem FreeRTOS/ESP-IDF: clock injetado via nb_arc_core_tick(dt_ms).
 */

typedef enum {
    NB_ARC_PHASE_IDLE = 0, /* nenhum arco rodando (pode estar em cooldown) */
    NB_ARC_PHASE_ORIENT,
    NB_ARC_PHASE_EXECUTE,
    NB_ARC_PHASE_OUTCOME,
} nb_arc_phase_t;

typedef struct {
    nb_arc_phase_t phase;
    uint64_t now_ms;
    uint64_t phase_started_ms;
    uint64_t cooldown_until_ms;
    uint32_t orient_ms;
    uint32_t execute_ms;
    uint32_t outcome_ms;
    uint32_t cooldown_ms;
} nb_arc_state_t;

void nb_arc_core_init(nb_arc_state_t *arc);

/* Inicia o arco com as durações desta instância (cada arco concreto --
 * GRUMPY_FORGIVE, RECONCILE, SEARCH -- passa seus próprios valores).
 * Falha (retorna false, estado inalterado) se já está rodando ou ainda em
 * cooldown. */
bool nb_arc_core_start(nb_arc_state_t *arc, uint32_t orient_ms, uint32_t execute_ms,
                       uint32_t outcome_ms, uint32_t cooldown_ms);

/* Avança dt_ms; transiciona de fase quando a duração da fase atual esgota.
 * OUTCOME->IDLE arma o cooldown (cooldown_until_ms = now + cooldown_ms). */
void nb_arc_core_tick(nb_arc_state_t *arc, uint32_t dt_ms);

/* Aborto imediato (entrada em IDLE do corpo) -- volta pra NB_ARC_PHASE_IDLE
 * sem armar cooldown. Idempotente (abortar já-idle é no-op). */
void nb_arc_core_abort(nb_arc_state_t *arc);

bool nb_arc_core_is_running(const nb_arc_state_t *arc);

bool nb_arc_core_in_cooldown(const nb_arc_state_t *arc);

#ifdef __cplusplus
}
#endif

#endif
