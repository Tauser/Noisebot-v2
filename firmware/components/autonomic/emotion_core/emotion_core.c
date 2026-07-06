#include "emotion_core.h"

#include <math.h>
#include <stddef.h>

/* Constante de decaimento derivada de BEHAVIOR.md §2 ("constante ~60s até
 * <5% do pico"): exp(-60/tau) = 0.05 => tau = 60/ln(20) ≈ 20.0285s. */
#define NB_EMOTION_DECAY_TAU_S 20.0285f

/* Temperamento (S3.7 completo, item 6, RFC-VIDA-V2.md §7): "em paz, o
 * Noise é sutilmente caloroso e desperto" -- alvo do decay em vez de
 * (0,0). Consequência visível: o micro-sorriso do NEUTRAL (RFC §3.1). */
#define NB_EMOTION_TEMPERAMENT_VALENCE 0.10f
#define NB_EMOTION_TEMPERAMENT_AROUSAL 0.05f

/* Distância mínima (ao quadrado) pra tratar o vetor como "exatamente" numa
 * âncora -- evita divisão por zero no blend por distância inversa e
 * garante bit-a-bit igual à âncora quando o vetor coincide (RFC §9). */
#define NB_EMOTION_HUB_EPSILON_SQ 1e-8f

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
    state->circadian_arousal_offset = 0.0f;
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

void nb_emotion_core_set_circadian_offset(nb_emotion_state_t *state, float arousal_offset)
{
    if (state == NULL) {
        return;
    }
    state->circadian_arousal_offset = arousal_offset;
}

void nb_emotion_core_tick(nb_emotion_state_t *state, uint32_t dt_ms)
{
    if (state == NULL) {
        return;
    }
    const float decay = expf(-((float)dt_ms / 1000.0f) / NB_EMOTION_DECAY_TAU_S);
    const float target_arousal =
        NB_EMOTION_TEMPERAMENT_AROUSAL + state->circadian_arousal_offset;

    state->valence = NB_EMOTION_TEMPERAMENT_VALENCE +
                     (state->valence - NB_EMOTION_TEMPERAMENT_VALENCE) * decay;
    state->arousal = target_arousal + (state->arousal - target_arousal) * decay;
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

/* Só os 4 hubs (RFC-VIDA-V2.md §3.1) participam do campo contínuo -- as
 * outras 6 âncoras saem de uso (decisão de produto, 2026-07-06). */
static const nb_face_expr_t s_hub_exprs[4] = {
    NB_FACE_EXPR_NEUTRAL,
    NB_FACE_EXPR_HAPPY,
    NB_FACE_EXPR_SAD,
    NB_FACE_EXPR_ANGRY,
};

void nb_emotion_core_resolve_face(const nb_emotion_state_t *state, nb_face_state_t *out)
{
    if (out == NULL) {
        return;
    }
    if (state == NULL) {
        *out = *nb_face_core_get_expression(NB_FACE_EXPR_NEUTRAL);
        return;
    }

    float dist_sq[4];
    const nb_face_state_t *hub_faces[4];

    for (uint32_t i = 0; i < 4u; ++i) {
        const nb_emotion_anchor_t *anchor = &s_anchors[s_hub_exprs[i]];
        const float dv = state->valence - anchor->valence;
        const float da = state->arousal - anchor->arousal;

        dist_sq[i] = dv * dv + da * da;
        hub_faces[i] = nb_face_core_get_expression(s_hub_exprs[i]);

        if (dist_sq[i] < NB_EMOTION_HUB_EPSILON_SQ) {
            /* Vetor "exatamente" na âncora -- bit-a-bit igual a ela, sem
             * passar pelo blend (RFC §9: "campo passa exatamente pelas
             * âncoras calibradas"). */
            *out = *hub_faces[i];
            return;
        }
    }

    float weights[4];
    float weight_sum = 0.0f;

    for (uint32_t i = 0; i < 4u; ++i) {
        weights[i] = 1.0f / dist_sq[i]; /* Shepard, potência 2 */
        weight_sum += weights[i];
    }
    for (uint32_t i = 0; i < 4u; ++i) {
        weights[i] /= weight_sum;
    }

    nb_face_core_blend(hub_faces, weights, 4u, out);
}
