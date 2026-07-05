#ifndef NB_REFLEX_ENGINE_H
#define NB_REFLEX_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

#include "tiny_fsm.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro do arbitrador de estímulo→reação (S3.2, BEHAVIOR.md §2-3).
 *
 * Duas tabelas fixas, travadas por host-test de regressão:
 *  - estímulo→delta afetivo (BEHAVIOR §2, sempre integrado em emotion_core
 *    pela casca, independente de quem está no controle do display);
 *  - estímulo→claim de prioridade (BEHAVIOR §3): P0 safety > P1 touch >
 *    P2 fala > P3 hint. P4 (emoção mapeada), P5 (motifs de idle) e P6
 *    (baseline) são resolvidos pela casca quando nenhuma claim P0-P3 está
 *    ativa -- emotion_core e idle_engine são L4 irmãos, não dependências
 *    deste núcleo (ARCHITECTURE.md §2, evita acoplamento cruzado entre
 *    núcleos puros).
 *
 * Claim ativa suprime as inferiores sem destruí-las: ao expirar, a banda
 * de baixo reaparece sozinha (a casca só precisa reconsultar a prioridade
 * ativa a cada tick). Conflito na mesma banda: a claim mais recente vence
 * (sobrescreve expiração e evento).
 *
 * Sem FreeRTOS/ESP-IDF/event_bus: estímulos chegam por chamada direta de
 * nb_reflex_engine_on_stimulus(); dt_ms e leitura de toque injetados via
 * nb_reflex_engine_tick().
 */

#define NB_REFLEX_CLAIM_BANDS 4u /* P0 safety, P1 touch, P2 fala, P3 hint */

typedef enum {
    NB_REFLEX_PRIORITY_SAFETY = 0,
    NB_REFLEX_PRIORITY_TOUCH = 1,
    NB_REFLEX_PRIORITY_SPEECH = 2,
    NB_REFLEX_PRIORITY_HINT = 3,
    NB_REFLEX_PRIORITY_EMOTION = 4,
    NB_REFLEX_PRIORITY_IDLE_MOTIF = 5,
    NB_REFLEX_PRIORITY_BASELINE = 6,
} nb_reflex_priority_t;

/* Nenhuma claim P0-P3 ativa -- a casca resolve P4/P5/P6. */
#define NB_REFLEX_PRIORITY_UNCLAIMED NB_REFLEX_PRIORITY_BASELINE

/* 13 estímulos locais de BEHAVIOR.md §2 (herdados do v1) + TOUCH_RELEASE
 * (retorno suave ao soltar, §4) + TOUCH_WAKE (toque durante sleeping). */
typedef enum {
    NB_REFLEX_STIMULUS_TOUCH_TAP = 0,
    NB_REFLEX_STIMULUS_TOUCH_LONG,
    NB_REFLEX_STIMULUS_TOUCH_WARM_PULSE,
    NB_REFLEX_STIMULUS_TOUCH_DEEP,
    NB_REFLEX_STIMULUS_TOUCH_CARESS,
    NB_REFLEX_STIMULUS_TOUCH_RELEASE,
    NB_REFLEX_STIMULUS_TOUCH_WAKE,
    NB_REFLEX_STIMULUS_VOICE_START,
    NB_REFLEX_STIMULUS_VOICE_LOUD,
    NB_REFLEX_STIMULUS_VOICE_SOFT,
    NB_REFLEX_STIMULUS_AUDIO_PLAYING,
    NB_REFLEX_STIMULUS_ENTERING_SLEEP,
    NB_REFLEX_STIMULUS_WAKING_UP,
    NB_REFLEX_STIMULUS_MOTION_FAULT,
    NB_REFLEX_STIMULUS_IDLE_LONG,
    NB_REFLEX_STIMULUS_COUNT,
} nb_reflex_stimulus_t;

typedef struct {
    bool active;
    uint32_t expires_at_ms;
    uint32_t activated_at_ms;
    nb_fsm_event_t fsm_event; /* NB_FSM_EVENT_COUNT = nenhum */
} nb_reflex_claim_t;

typedef struct {
    nb_reflex_claim_t claims[NB_REFLEX_CLAIM_BANDS]; /* indexado por nb_reflex_priority_t P0..P3 */
    uint32_t now_ms;

    nb_reflex_stimulus_t touch_current_phase; /* NB_REFLEX_STIMULUS_COUNT = nenhuma */
    bool touch_was_pressed;
    uint32_t last_touch_phase_stimulus_at_ms;
} nb_reflex_engine_t;

typedef struct {
    nb_reflex_priority_t active_priority;
    nb_fsm_event_t fsm_event; /* NB_FSM_EVENT_COUNT se nenhum evento novo neste tick */
    bool has_affect_delta;
    float delta_valence;
    float delta_arousal;
} nb_reflex_reaction_t;

void nb_reflex_engine_init(nb_reflex_engine_t *engine);

/* Aplica um estímulo discreto agora (now_ms): popula sempre o delta afetivo
 * da tabela (mesmo 0,0) e, se o estímulo tiver banda de prioridade,
 * abre/renova a claim daquela banda (sobrescreve se já havia uma -- mais
 * recente vence). NULL-safe. */
void nb_reflex_engine_on_stimulus(nb_reflex_engine_t *engine, nb_reflex_stimulus_t stimulus,
                                  uint32_t now_ms, nb_reflex_reaction_t *out_reaction);

/* Avança dt_ms: reclassifica toque contínuo por duração real do
 * touch_service (TAP/LONG já chegam discretos via on_stimulus; aqui só
 * WARM_PULSE 3-8s [repetindo a cada 4s]/DEEP 8-15s/CARESS >15s), dispara
 * TOUCH_RELEASE ao ver a borda de descida, e expira claims vencidas. */
void nb_reflex_engine_tick(nb_reflex_engine_t *engine, uint32_t dt_ms, bool touch_is_pressed,
                           uint32_t touch_duration_ms, nb_reflex_reaction_t *out_reaction);

nb_reflex_priority_t nb_reflex_engine_get_active_priority(const nb_reflex_engine_t *engine,
                                                          uint32_t now_ms);

/* Zera toda a pilha de claims imediatamente, ignorando expiração --
 * chamado pela casca sempre que tiny_fsm pousa em IDLE (invariante X→IDLE,
 * ARCHITECTURE.md §4: reflex_engine nunca disputa com ela). */
void nb_reflex_engine_force_clear(nb_reflex_engine_t *engine);

#ifdef __cplusplus
}
#endif

#endif
