#ifndef NB_ATTENTION_H
#define NB_ATTENTION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro -- motor de atenção do idle v2 (S3.7 spike,
 * docs/RFC-VIDA-V2.md §7, motor 1 "Atenção"). FSM FIXATE<->SACCADE:
 * fixação de 0.5-3s com micro-tremor (olhos que "parecem ver"), sacada de
 * 80ms (ease-out) para um alvo em [-0.35,0.35]^2, com ~40% de viés de
 * retorno ao centro ("olhar pro usuário"). Subsume SOFT_DRIFT/SIDE_PEEK/
 * scans do catálogo de motifs de S2.4 (RFC §7).
 *
 * RNG embutido (xorshift32, mesma técnica de idle_engine.c) -- sem
 * FreeRTOS/ESP-IDF, clock injetado via nb_attention_tick(dt_ms).
 */

#define NB_ATTENTION_ENVELOPE 0.35f
#define NB_ATTENTION_CENTER_BIAS_RADIUS 0.05f
#define NB_ATTENTION_CENTER_BIAS_PROB 0.40f
#define NB_ATTENTION_FIXATION_MIN_MS 500u
#define NB_ATTENTION_FIXATION_MAX_MS 3000u
#define NB_ATTENTION_SACCADE_MS 80u
#define NB_ATTENTION_TREMOR_AMPLITUDE 0.02f

typedef enum {
    NB_ATTENTION_FIXATE = 0,
    NB_ATTENTION_SACCADE = 1,
} nb_attention_phase_t;

/* Disparado no instante em que uma sacada começa -- gancho para a casca
 * acoplar o blink à sacada (RFC §7, "acoplamentos: blink×sacada"). Neste
 * spike o mecanismo só é exposto e testado; o acoplamento real (casca
 * disparando blink de fato) fica para o "Plano S3.7 completo" pós-go. */
typedef void (*nb_attention_saccade_cb_t)(void *ctx);

typedef struct {
    uint32_t rng_state;
    uint32_t now_ms;

    nb_attention_phase_t phase;
    uint32_t phase_started_ms;
    uint32_t phase_duration_ms;

    float from_x; /* origem da sacada em curso */
    float from_y;
    float target_x; /* alvo da fixação atual (sem tremor) */
    float target_y;

    nb_attention_saccade_cb_t on_saccade;
    void *on_saccade_ctx;
} nb_attention_t;

/* Semente 0 é substituída por um valor fixo não-zero (xorshift32 trava em
 * 0) -- mesma regra de idle_engine.c. Estado inicial: FIXATE no centro. */
void nb_attention_init(nb_attention_t *att, uint32_t rng_seed);

void nb_attention_set_saccade_callback(nb_attention_t *att, nb_attention_saccade_cb_t cb,
                                       void *ctx);

/* Avança dt_ms e escreve o gaze resolvido (alvo de fixação + tremor, ou
 * interpolação de sacada) em gaze_x/gaze_y -- sempre dentro de
 * [-NB_ATTENTION_ENVELOPE, NB_ATTENTION_ENVELOPE]. Ponteiros de saída
 * podem ser NULL. */
void nb_attention_tick(nb_attention_t *att, uint32_t dt_ms, float *gaze_x, float *gaze_y);

#ifdef __cplusplus
}
#endif

#endif
