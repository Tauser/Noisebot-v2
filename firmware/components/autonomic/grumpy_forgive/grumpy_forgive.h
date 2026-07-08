#ifndef NB_GRUMPY_FORGIVE_H
#define NB_GRUMPY_FORGIVE_H

#include <stdbool.h>
#include <stdint.h>

#include "arc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro do arco GRUMPY_FORGIVE (S3.8, item 4, RFC-VIDA-V2.md
 * §5.2): "≥3 TAP em 10s -> ANGRY ~60% 1.2s -> blink lento + gaze desviado
 * 800ms -> NEUTRAL + micro-HAPPY 400ms. Cooldown 60s."
 *
 * Compõe nb_arc_state_t (arc_core, item 3) pra fase/tempo/cooldown; a
 * parte nova aqui é só a janela deslizante de TAP e o mapeamento fase+
 * tempo-dentro-da-fase -> "beat" visual que a casca consome. Sem
 * orientação visível (RFC não descreve uma) -- orient_ms=0 (arc_core
 * cascateia direto pra EXECUTE no primeiro tick).
 *
 * Sem FreeRTOS/ESP-IDF: tudo injetado. on_tap() recebe now_ms explícito
 * (mesmo padrão de nb_reflex_engine_on_stimulus) -- é o clock absoluto da
 * janela de 10s, independente do clock relativo interno do arco (que só
 * avança via tick(dt_ms), mesma convenção de arc_core).
 */

#define NB_GRUMPY_FORGIVE_TAP_THRESHOLD 3u
#define NB_GRUMPY_FORGIVE_TAP_WINDOW_MS 10000u

#define NB_GRUMPY_FORGIVE_EXECUTE_MS 1200u  /* ANGRY ~60% */
#define NB_GRUMPY_FORGIVE_GAZE_MS 800u      /* dentro do OUTCOME: blink lento + gaze desviado */
#define NB_GRUMPY_FORGIVE_NEUTRAL_MS 400u   /* dentro do OUTCOME: NEUTRAL + micro-HAPPY */
#define NB_GRUMPY_FORGIVE_OUTCOME_MS (NB_GRUMPY_FORGIVE_GAZE_MS + NB_GRUMPY_FORGIVE_NEUTRAL_MS)
#define NB_GRUMPY_FORGIVE_COOLDOWN_MS 60000u

typedef enum {
    NB_GRUMPY_FORGIVE_BEAT_NONE = 0,
    NB_GRUMPY_FORGIVE_BEAT_ANGRY,
    NB_GRUMPY_FORGIVE_BEAT_BLINK_GAZE_AWAY,
    NB_GRUMPY_FORGIVE_BEAT_NEUTRAL_HAPPY,
} nb_grumpy_forgive_beat_t;

typedef struct {
    nb_arc_state_t arc;
    uint64_t tap_times_ms[NB_GRUMPY_FORGIVE_TAP_THRESHOLD]; /* anel */
    uint32_t tap_count;     /* satura em NB_GRUMPY_FORGIVE_TAP_THRESHOLD */
    uint32_t tap_next_slot;
} nb_grumpy_forgive_state_t;

void nb_grumpy_forgive_init(nb_grumpy_forgive_state_t *state);

/* Registra um TAP em now_ms. Se as últimas NB_GRUMPY_FORGIVE_TAP_THRESHOLD
 * batidas couberem numa janela de NB_GRUMPY_FORGIVE_TAP_WINDOW_MS, tenta
 * iniciar o arco. Retorna true só se o arco de fato iniciou agora (false
 * se ainda não bateu o mínimo de taps na janela, ou se o arco já está
 * rodando/em cooldown -- arc_core recusa silenciosamente). */
bool nb_grumpy_forgive_on_tap(nb_grumpy_forgive_state_t *state, uint64_t now_ms);

void nb_grumpy_forgive_tick(nb_grumpy_forgive_state_t *state, uint32_t dt_ms);

/* Entrada em IDLE do corpo -- aborta o arco em qualquer fase, sem
 * cooldown (mesma regra de arc_core). */
void nb_grumpy_forgive_abort(nb_grumpy_forgive_state_t *state);

/* "Beat" visual do instante atual, derivado da fase do arco + tempo
 * decorrido dentro dela. NONE fora de ORIENT/EXECUTE/OUTCOME ativos. */
nb_grumpy_forgive_beat_t nb_grumpy_forgive_current_beat(const nb_grumpy_forgive_state_t *state);

#ifdef __cplusplus
}
#endif

#endif
