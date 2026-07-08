#ifndef NB_RECONCILE_H
#define NB_RECONCILE_H

#include <stdbool.h>
#include <stdint.h>

#include "arc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro do arco RECONCILE (S3.8, item 5, RFC-VIDA-V2.md §5.2):
 * "vetor em região negativa + carinho (LONG/CARESS) -> gaze busca a
 * frente ('te olha') -> suavização 1.5s -> CONTENT + SLOW_BLINK (+ ♥ se
 * o carinho continua). Termina mais positivo que o repouso -- fazer as
 * pazes aproxima. Depende de CONTENT (§3.2, item 1)."
 *
 * Compõe nb_arc_state_t (arc_core, item 3): ORIENT="gaze busca a frente"
 * (~300ms, sem número no RFC -- mesma convenção usada em SEARCH, item 6,
 * retunar em bancada), EXECUTE="suavização" (1.5s, explícito no RFC),
 * OUTCOME="CONTENT + SLOW_BLINK" (650ms, mesma duração de
 * NB_IDLE_SLOW_BLINK_DURATION_MS do S3.7 -- valor espelhado, não
 * importado, pra não criar dependência cruzada entre núcleos autonômicos
 * irmãos, ARCHITECTURE.md §2).
 *
 * Ao contrário de GRUMPY_FORGIVE (gatilho por evento discreto, contagem
 * de TAP), o gatilho aqui é uma condição de nível sustentada
 * (valence<0 && carinho ativo), reavaliada a cada tick pela casca --
 * dessa forma nb_reconcile_tick() recebe o snapshot do frame e decide
 * start/abort sozinho, num único ponto de entrada por frame.
 *
 * Sem cooldown (cooldown_ms=0 pro arc_core): o próprio gatilho se
 * autolimita -- ao terminar em CONTENT, valence fica positivo (aplicado
 * pela casca em emotion_core), então a condição de disparo deixa de valer
 * sozinha; nenhum cooldown artificial é necessário (RFC não pede um).
 *
 * "Zero âncora nova" (RFC §5.2): este núcleo não conhece emotion_core/
 * touch_service -- valence_negative/caress_active chegam como bool já
 * decidido pela casca a cada frame.
 */

#define NB_RECONCILE_ORIENT_MS 300u   /* "gaze busca a frente" */
#define NB_RECONCILE_EXECUTE_MS 1500u /* "suavização" (RFC, explícito) */
#define NB_RECONCILE_OUTCOME_MS 650u  /* "CONTENT + SLOW_BLINK" */

typedef enum {
    NB_RECONCILE_BEAT_NONE = 0,
    NB_RECONCILE_BEAT_GAZE_FRONT,
    NB_RECONCILE_BEAT_SMOOTHING,
    NB_RECONCILE_BEAT_CONTENT_SLOW_BLINK,
    /* Igual ao anterior + glifo de coração -- só quando o carinho ainda
     * estava ativo no instante exato em que o arco entrou em OUTCOME. */
    NB_RECONCILE_BEAT_CONTENT_SLOW_BLINK_HEART,
} nb_reconcile_beat_t;

typedef struct {
    nb_arc_state_t arc;
    bool caress_active_at_outcome_entry;
} nb_reconcile_state_t;

void nb_reconcile_init(nb_reconcile_state_t *state);

/* Avança dt_ms com o snapshot do frame (valence_negative = vetor de
 * emotion_core em região negativa; caress_active = toque sustentado tipo
 * LONG/CARESS, RFC "carinho"). Decide start/abort sozinho:
 *  - parado + (valence_negative && caress_active): inicia o arco.
 *  - rodando em ORIENT/EXECUTE + caress_active virou false: aborta (RFC
 *    "aborta limpo se o carinho parar antes da suavização" -- suavização
 *    é o EXECUTE; antes dela inclui o ORIENT).
 *  - rodando em OUTCOME: carinho pode parar sem abortar -- a suavização
 *    já aconteceu; só decide se o glifo de coração aparece (snapshot
 *    tirado na entrada do OUTCOME, não muda depois). */
void nb_reconcile_tick(nb_reconcile_state_t *state, uint32_t dt_ms, bool valence_negative,
                       bool caress_active);

/* Entrada em IDLE do corpo -- aborta em qualquer fase, sem cooldown
 * (mesma regra de arc_core; aqui já não há cooldown de qualquer forma). */
void nb_reconcile_abort(nb_reconcile_state_t *state);

nb_reconcile_beat_t nb_reconcile_current_beat(const nb_reconcile_state_t *state);

#ifdef __cplusplus
}
#endif

#endif
