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

/* Decay assimétrico (S3.8, item 2, RFC-VIDA-V2.md §5.1): "regiões
 * negativas decaem ~2x mais devagar -- mágoa que evapora sozinha
 * banaliza o consolo". Só o eixo de valência é assimétrico (é o eixo de
 * "mágoa"/vínculo, RFC §5.2 RECONCILE fala em "vetor em região
 * negativa" no sentido de valência); ativação continua com o tau
 * simétrico original. Enquanto valence < 0 usa este tau (2x maior);
 * assim que cruza pra positivo, volta ao tau normal -- sem descontinuidade
 * na fórmula, só troca de constante tick a tick. */
#define NB_EMOTION_NEGATIVE_DECAY_TAU_S (NB_EMOTION_DECAY_TAU_S * 2.0f)

/* Temperamento (S3.7 completo, item 6, RFC-VIDA-V2.md §7): "em paz, o
 * Noise é sutilmente caloroso e desperto" -- alvo do decay em vez de
 * (0,0). Consequência visível: o micro-sorriso do NEUTRAL (RFC §3.1). */
#define NB_EMOTION_TEMPERAMENT_VALENCE 0.10f
#define NB_EMOTION_TEMPERAMENT_AROUSAL 0.05f

/* Distância mínima (ao quadrado) pra tratar o vetor como "exatamente" numa
 * âncora -- evita divisão por zero no blend por distância inversa e
 * garante bit-a-bit igual à âncora quando o vetor coincide (RFC §9). */
#define NB_EMOTION_HUB_EPSILON_SQ 1e-8f

/* Boca é canal de intensidade (RFC-VIDA-V2.md §3.1a): histerese -- entra
 * em >=0.40, sai em <0.30 (nunca pisca na fronteira). Entre a saída
 * (0.30) e o pico (~0.70) a escala visual sobe continuamente; abaixo da
 * saída, sempre zero. Intensidade = norma do vetor (v,a). */
#define NB_EMOTION_MOUTH_ENTER_INTENSITY 0.40f
#define NB_EMOTION_MOUTH_EXIT_INTENSITY 0.30f
#define NB_EMOTION_MOUTH_PEAK_INTENSITY 0.70f

/* Âncoras (valência, ativação) das 5 expressões-base -- mesmo
 * `nb_face_expr_t` do renderer, sem duplicar a tabela. Histórico tinha 10
 * (VISUAL.md §2); S3.8 item 9 aposentou as 6 fora dos hubs. */
typedef struct {
    float valence;
    float arousal;
} nb_emotion_anchor_t;

static const nb_emotion_anchor_t s_anchors[NB_FACE_EXPR_COUNT] = {
    {0.0f, 0.0f},   /* NEUTRAL */
    {0.7f, 0.3f},   /* HAPPY */
    {-0.6f, -0.4f}, /* SAD */
    {-0.8f, 0.6f},  /* ANGRY */
    {0.6f, -0.5f},  /* CONTENT (S3.8, item 1) */
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

void nb_emotion_core_set_mouth_forced(nb_emotion_state_t *state, bool forced)
{
    if (state == NULL) {
        return;
    }
    state->mouth_forced = forced;
}

void nb_emotion_core_tick(nb_emotion_state_t *state, uint32_t dt_ms)
{
    if (state == NULL) {
        return;
    }
    const float dt_s = (float)dt_ms / 1000.0f;
    const float valence_tau =
        (state->valence < 0.0f) ? NB_EMOTION_NEGATIVE_DECAY_TAU_S : NB_EMOTION_DECAY_TAU_S;
    const float valence_decay = expf(-dt_s / valence_tau);
    const float arousal_decay = expf(-dt_s / NB_EMOTION_DECAY_TAU_S);
    const float target_arousal =
        NB_EMOTION_TEMPERAMENT_AROUSAL + state->circadian_arousal_offset;

    state->valence = NB_EMOTION_TEMPERAMENT_VALENCE +
                     (state->valence - NB_EMOTION_TEMPERAMENT_VALENCE) * valence_decay;
    state->arousal = target_arousal + (state->arousal - target_arousal) * arousal_decay;
}

/* 5 hubs (RFC-VIDA-V2.md §3.1 + §3.2 item 1, CONTENT) participam do campo
 * contínuo -- as outras 6 âncoras saem de uso (decisão de produto,
 * 2026-07-06, removidas de vez no item 9 do plano S3.8). */
#define NB_EMOTION_HUB_COUNT 5u
static const nb_face_expr_t s_hub_exprs[NB_EMOTION_HUB_COUNT] = {
    NB_FACE_EXPR_NEUTRAL,
    NB_FACE_EXPR_HAPPY,
    NB_FACE_EXPR_SAD,
    NB_FACE_EXPR_ANGRY,
    NB_FACE_EXPR_CONTENT,
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

    case NB_FACE_EXPR_CONTENT:
        if (variant == NB_EMOTION_VARIANT_A) {
            /* "sereno": pálpebra ainda mais relaxada. */
            out->open_l -= 0.10f * weight;
            out->open_r -= 0.10f * weight;
        } else {
            /* "caloroso": sorriso um traço mais aberto. */
            out->mouth_open += 0.10f * weight;
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

/* RFC-VIDA-V2.md §3.1a: boca é canal de intensidade, não traço permanente.
 * Histerese sobre a norma do vetor decide se a boca existe; entre a saída
 * (0.30) e o pico (0.70) a escala sobe continuamente. `mouth_forced`
 * (fala/arco) ignora tudo isso e mantém a boca cheia. Aplicado por cima do
 * resultado final do blend+variante -- escala mouth_open/mouth_curve,
 * nunca os outros campos (a intensidade não apaga os olhos). */
static void apply_mouth_gate(nb_emotion_state_t *state, nb_face_state_t *out)
{
    if (state->mouth_forced) {
        return;
    }

    const float intensity = sqrtf(state->valence * state->valence + state->arousal * state->arousal);

    if (!state->mouth_active && intensity >= NB_EMOTION_MOUTH_ENTER_INTENSITY) {
        state->mouth_active = true;
    } else if (state->mouth_active && intensity < NB_EMOTION_MOUTH_EXIT_INTENSITY) {
        state->mouth_active = false;
    }

    const float scale =
        state->mouth_active
            ? nb_emotion_core_clampf((intensity - NB_EMOTION_MOUTH_EXIT_INTENSITY) /
                                         (NB_EMOTION_MOUTH_PEAK_INTENSITY -
                                          NB_EMOTION_MOUTH_EXIT_INTENSITY),
                                     0.0f, 1.0f)
            : 0.0f;

    out->mouth_open *= scale;
    out->mouth_curve *= scale;
}

void nb_emotion_core_resolve_face(nb_emotion_state_t *state, nb_face_state_t *out)
{
    if (out == NULL) {
        return;
    }
    if (state == NULL) {
        *out = *nb_face_core_get_expression(NB_FACE_EXPR_NEUTRAL);
        /* state==NULL -> sem histerese pra consultar; repouso nunca
         * mostra boca (RFC §3.1a item 2), então zera aqui também. */
        out->mouth_open = 0.0f;
        out->mouth_curve = 0.0f;
        return;
    }

    float dist_sq[NB_EMOTION_HUB_COUNT];
    const nb_face_state_t *hub_faces[NB_EMOTION_HUB_COUNT];
    uint32_t dominant_idx = 0;

    for (uint32_t i = 0; i < NB_EMOTION_HUB_COUNT; ++i) {
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
        apply_mouth_gate(state, out);
        return;
    }

    float weights[NB_EMOTION_HUB_COUNT];
    float weight_sum = 0.0f;

    for (uint32_t i = 0; i < NB_EMOTION_HUB_COUNT; ++i) {
        weights[i] = 1.0f / dist_sq[i]; /* Shepard, potência 2 */
        weight_sum += weights[i];
    }
    for (uint32_t i = 0; i < NB_EMOTION_HUB_COUNT; ++i) {
        weights[i] /= weight_sum;
    }

    nb_face_core_blend(hub_faces, weights, NB_EMOTION_HUB_COUNT, out);
    apply_variant(dominant_hub, state->active_variant, weights[dominant_idx], out);
    apply_mouth_gate(state, out);
}
