#ifndef NB_EMOTION_CORE_H
#define NB_EMOTION_CORE_H

#include "renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro do modelo emocional v0 (S2.5, BEHAVIOR.md §2).
 *
 * Vetor contínuo 2D (valência × ativação), clamp em ±1. Estímulos somam
 * deltas (clampados); decaimento exponencial para (0,0) com constante tal
 * que o pico cai a <5% em ~60s (BEHAVIOR.md §2). O vetor mapeia por
 * nearest-neighbor às âncoras das 10 expressões-base (VISUAL.md §2, mesmo
 * enum `nb_face_expr_t` do renderer -- S2.5 é L4, chama L3 adjacente,
 * `ARCHITECTURE.md` §2, sem duplicar a tabela de expressões).
 *
 * Sem FreeRTOS/ESP-IDF/NVS: clock injetado via nb_emotion_core_tick(dt_ms).
 * Persistência da última expressão (BEHAVIOR.md §2 "Persistência") fica
 * para a casca, quando existir -- fora do escopo desta fatia.
 */

typedef struct {
    float valence; /* [-1, +1] */
    float arousal; /* [-1, +1] */
} nb_emotion_state_t;

void nb_emotion_core_init(nb_emotion_state_t *state);

/* Soma um estímulo {delta_valence, delta_arousal} ao vetor, clampando o
 * resultado em [-1, +1] em cada eixo (BEHAVIOR.md §2: "delta é clampeado
 * por tick"). */
void nb_emotion_core_apply_stimulus(nb_emotion_state_t *state, float delta_valence,
                                    float delta_arousal);

/* Avança dt_ms com decaimento exponencial rumo a (0,0). */
void nb_emotion_core_tick(nb_emotion_state_t *state, uint32_t dt_ms);

/* Expressão-âncora mais próxima do vetor atual (distância euclidiana no
 * plano valência×ativação, VISUAL.md §2). Nunca falha -- sempre retorna
 * uma das NB_FACE_EXPR_COUNT expressões. */
nb_face_expr_t nb_emotion_core_nearest_expression(const nb_emotion_state_t *state);

float nb_emotion_core_clampf(float v, float lo, float hi);

#ifdef __cplusplus
}
#endif

#endif
