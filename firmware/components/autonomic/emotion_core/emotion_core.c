#include "emotion_core.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static uint32_t xorshift32(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/* [0, 1) com 24 bits de precisão -- mesma técnica dos núcleos do
 * idle_engine (nb_attention.c etc.). */
static float rand01(uint32_t *state)
{
    return (float)(xorshift32(state) >> 8) / (float)(1u << 24);
}

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

void nb_emotion_core_init(nb_emotion_state_t *state, uint32_t rng_seed)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->rng_state = (rng_seed != 0u) ? rng_seed : 0x9E3779B9u;
    state->dominant_hub = NB_FACE_EXPR_NEUTRAL;
    state->active_variant = NB_EMOTION_VARIANT_A;
    state->has_dominant_hub = false;
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

/* Variante episódica (S3.7 completo, item 7, RFC-VIDA-V2.md §3.1): tempera
 * o resultado do blend, escalado por `weight` (o peso do hub dominante --
 * esmaece com ele, nunca um salto na troca de episódio). Deltas práticos
 * (RFC não dá números pras variantes, só a intenção qualitativa) --
 * retunar em bancada, mesma classe de NB_IDLE_DRIFT_AMPLITUDE. */
static void apply_variant(nb_face_expr_t hub, nb_emotion_variant_t variant, float weight,
                          nb_face_state_t *out)
{
    switch (hub) {
    case NB_FACE_EXPR_NEUTRAL:
        if (variant == NB_EMOTION_VARIANT_A) {
            /* "sereno": pálpebra ~5% mais baixa. */
            out->open_l -= 0.05f * weight;
            out->open_r -= 0.05f * weight;
        } else {
            /* "atento": gaze um traço mais alto (y_l/y_r mais negativo). */
            out->y_l -= 0.05f * weight;
            out->y_r -= 0.05f * weight;
        }
        break;

    case NB_FACE_EXPR_HAPPY:
        if (variant == NB_EMOTION_VARIANT_A) {
            /* "radiante": boca mais aberta/cheia. */
            out->mouth_open += 0.15f * weight;
        } else {
            /* "contido": sorriso mais fechado. */
            out->mouth_open -= 0.15f * weight;
        }
        break;

    case NB_FACE_EXPR_SAD:
        if (variant == NB_EMOTION_VARIANT_A) {
            /* "murcho": gaze baixo. */
            out->y_l += 0.05f * weight;
            out->y_r += 0.05f * weight;
        } else {
            /* "magoado": assimetria, evita olhar de frente. */
            out->x_off += 0.10f * weight;
        }
        break;

    case NB_FACE_EXPR_ANGRY:
        if (variant == NB_EMOTION_VARIANT_A) {
            /* "irritado": squint parcial (menos que o ANGRY base). */
            out->squint_l *= (1.0f - 0.3f * weight);
            out->squint_r *= (1.0f - 0.3f * weight);
        } else {
            /* "bravo": squint cheio (mais que o ANGRY base). */
            out->squint_l += 0.15f * weight;
            out->squint_r += 0.15f * weight;
        }
        break;

    default:
        break;
    }

    /* Envelope: a variante tempera, nunca sai dos limites válidos do
     * campo (RFC §9(5): "nunca sai do envelope da região"). */
    out->open_l = nb_emotion_core_clampf(out->open_l, 0.0f, 1.0f);
    out->open_r = nb_emotion_core_clampf(out->open_r, 0.0f, 1.0f);
    out->mouth_open = nb_emotion_core_clampf(out->mouth_open, 0.0f, 1.0f);
    out->squint_l = nb_emotion_core_clampf(out->squint_l, 0.0f, 2.0f);
    out->squint_r = nb_emotion_core_clampf(out->squint_r, 0.0f, 2.0f);
}

void nb_emotion_core_resolve_face(nb_emotion_state_t *state, nb_face_state_t *out)
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
    uint32_t dominant_idx = 0;

    for (uint32_t i = 0; i < 4u; ++i) {
        const nb_emotion_anchor_t *anchor = &s_anchors[s_hub_exprs[i]];
        const float dv = state->valence - anchor->valence;
        const float da = state->arousal - anchor->arousal;

        dist_sq[i] = dv * dv + da * da;
        hub_faces[i] = nb_face_core_get_expression(s_hub_exprs[i]);
        if (dist_sq[i] < dist_sq[dominant_idx]) {
            dominant_idx = i;
        }
    }

    const nb_face_expr_t dominant_hub = s_hub_exprs[dominant_idx];
    if (!state->has_dominant_hub || dominant_hub != state->dominant_hub) {
        state->active_variant =
            (rand01(&state->rng_state) < 0.5f) ? NB_EMOTION_VARIANT_A : NB_EMOTION_VARIANT_B;
        state->dominant_hub = dominant_hub;
        state->has_dominant_hub = true;
    }

    if (dist_sq[dominant_idx] < NB_EMOTION_HUB_EPSILON_SQ) {
        /* Vetor "exatamente" na âncora -- parte da âncora bit-a-bit (RFC
         * §9: "campo passa exatamente pelas âncoras calibradas"), com a
         * variante em peso 1.0 (é a única contribuição possível aqui). */
        *out = *hub_faces[dominant_idx];
        apply_variant(dominant_hub, state->active_variant, 1.0f, out);
        return;
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
    apply_variant(dominant_hub, state->active_variant, weights[dominant_idx], out);
}
