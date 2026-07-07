#ifndef NB_POSTURE_H
#define NB_POSTURE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro -- motor de postura da S3.7 completa (item 1 do plano
 * pós-go, docs/RFC-VIDA-V2.md §7, motor 2 "Postura"). FSM HOLD<->
 * TRANSITION: baseline que deriva a cada 30-90s (transição ~400ms
 * ease-out) pra uma nova micro-pose que vira o novo repouso -- nunca
 * retorna à pose exata.
 *
 * RNG embutido (xorshift32, mesma técnica de nb_attention.c) -- sem
 * FreeRTOS/ESP-IDF, clock injetado via nb_posture_tick(dt_ms).
 *
 * Amplitudes praticas (retunar em bancada quando a casca existir, mesmo
 * espírito de NB_IDLE_DRIFT_AMPLITUDE/NB_ATTENTION_TREMOR_AMPLITUDE): os
 * literais do RFC (roll <=1 grau, gaze offset <=0.05, assimetria <=3%)
 * são ponto de partida, não valor final confirmado ao vivo.
 */

#define NB_POSTURE_HOLD_MIN_MS 30000u
#define NB_POSTURE_HOLD_MAX_MS 90000u
#define NB_POSTURE_TRANSITION_MS 400u
/* Mesma escala do campo "tilt" de nb_idle_output_t -- a deriva contínua
 * de postura deve ser sutil, bem menor que um gesto sustentado. */
#define NB_POSTURE_ROLL_AMPLITUDE 0.03f
#define NB_POSTURE_GAZE_OFFSET_AMPLITUDE 0.05f
/* Diferencial aplicado a width_l/width_r (width_l = 1+asymmetry, width_r
 * = 1-asymmetry) em idle_engine.c compute_output(). */
#define NB_POSTURE_ASYMMETRY_AMPLITUDE 0.03f

typedef enum {
    NB_POSTURE_HOLD = 0,
    NB_POSTURE_TRANSITION = 1,
} nb_posture_phase_t;

typedef struct {
    uint32_t rng_state;
    uint32_t now_ms;

    nb_posture_phase_t phase;
    uint32_t phase_started_ms;
    uint32_t phase_duration_ms;

    float from_roll;
    float from_gaze_x;
    float from_gaze_y;
    float from_asymmetry;
    float target_roll;
    float target_gaze_x;
    float target_gaze_y;
    float target_asymmetry;

    /* Saída resolvida do tick -- soma-se aos campos correspondentes de
     * nb_idle_output_t na casca do idle_engine. */
    float roll;
    float gaze_x;
    float gaze_y;
    float asymmetry;
} nb_posture_t;

/* Semente 0 é substituída por um valor fixo não-zero (xorshift32 trava em
 * 0) -- mesma regra de nb_attention.c. Estado inicial: HOLD no centro. */
void nb_posture_init(nb_posture_t *p, uint32_t rng_seed);

/* Invariante H7 (ARCHITECTURE.md §4, BEHAVIOR.md §1.1): zera a pose pro
 * centro (roll/gaze offset/assimetria em 0) e recomeça a deriva dali,
 * sem perder o relógio (`now_ms`) nem o estado do RNG. A casca chama isto
 * ao entrar em IDLE -- gancho ainda não fiado em main.c nesta etapa
 * (ver "Plano S3.7 completo", item 3 "acoplamentos"). */
void nb_posture_reset_to_center(nb_posture_t *p);

void nb_posture_tick(nb_posture_t *p, uint32_t dt_ms);

#ifdef __cplusplus
}
#endif

#endif
