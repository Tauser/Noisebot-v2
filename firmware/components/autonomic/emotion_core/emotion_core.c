#include "emotion_core.h"

#include <math.h>

/* Constante de decaimento derivada de BEHAVIOR.md §2 ("constante ~60s até
 * <5% do pico"): exp(-60/tau) = 0.05 => tau = 60/ln(20) ≈ 20.0285s. */
#define NB_EMOTION_DECAY_TAU_S 20.0285f

/* Âncoras (valência, ativação) das 10 expressões-base, VISUAL.md §2 --
 * mesmo `nb_face_expr_t` do renderer, sem duplicar a tabela. */
typedef struct {
    float valence;
    float arousal;
} nb_emotion_anchor_t;

static const nb_emotion_anchor_t s_anchors[NB_FACE_EXPR_COUNT] = {
    {0.0f, 0.0f},   /* NEUTRAL */
    {0.7f, 0.3f},   /* HAPPY */
    {0.3f, 0.4f},   /* CURIOUS */
    {0.0f, -0.8f},  /* SLEEPY */
    {0.1f, 0.6f},   /* FOCUSED */
    {-0.3f, 0.2f},  /* SUSPICIOUS */
    {0.0f, 0.8f},   /* SURPRISED */
    {-0.6f, -0.4f}, /* SAD */
    {-0.5f, 0.9f},  /* ALARMED */
    {-0.8f, 0.6f},  /* ANGRY */
};

float nb_emotion_core_clampf(float v, float lo, float hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

void nb_emotion_core_init(nb_emotion_state_t *state)
{
    if (state == NULL) {
        return;
    }
    state->valence = 0.0f;
    state->arousal = 0.0f;
}

void nb_emotion_core_apply_stimulus(nb_emotion_state_t *state, float delta_valence,
                                    float delta_arousal)
{
    if (state == NULL) {
        return;
    }
    state->valence = nb_emotion_core_clampf(state->valence + delta_valence, -1.0f, 1.0f);
    state->arousal = nb_emotion_core_clampf(state->arousal + delta_arousal, -1.0f, 1.0f);
}

void nb_emotion_core_tick(nb_emotion_state_t *state, uint32_t dt_ms)
{
    if (state == NULL) {
        return;
    }
    const float decay = expf(-((float)dt_ms / 1000.0f) / NB_EMOTION_DECAY_TAU_S);
    state->valence *= decay;
    state->arousal *= decay;
}

nb_face_expr_t nb_emotion_core_nearest_expression(const nb_emotion_state_t *state)
{
    if (state == NULL) {
        return NB_FACE_EXPR_NEUTRAL;
    }

    uint32_t best_idx = 0;
    float best_dist_sq = -1.0f;

    for (uint32_t i = 0; i < NB_FACE_EXPR_COUNT; ++i) {
        const float dv = state->valence - s_anchors[i].valence;
        const float da = state->arousal - s_anchors[i].arousal;
        const float dist_sq = dv * dv + da * da;

        if (best_dist_sq < 0.0f || dist_sq < best_dist_sq) {
            best_dist_sq = dist_sq;
            best_idx = i;
        }
    }
    return (nb_face_expr_t)best_idx;
}
