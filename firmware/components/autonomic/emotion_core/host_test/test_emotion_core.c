#include "../emotion_core.h"

#include <stddef.h>
#include <stdio.h>

static int failures;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);           \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static int float_eq(float a, float b, float tol)
{
    const float diff = (a > b) ? (a - b) : (b - a);
    return diff < tol;
}

static void test_init_is_neutral(void)
{
    nb_emotion_state_t state;

    nb_emotion_core_init(&state, 1u);
    CHECK(float_eq(state.valence, 0.0f, 0.0001f));
    CHECK(float_eq(state.arousal, 0.0f, 0.0001f));
    CHECK(nb_emotion_core_nearest_expression(&state) == NB_FACE_EXPR_NEUTRAL);
}

static void test_apply_stimulus_clamps_per_axis(void)
{
    nb_emotion_state_t state;

    nb_emotion_core_init(&state, 1u);
    nb_emotion_core_apply_stimulus(&state, 2.0f, -3.0f);
    CHECK(float_eq(state.valence, 1.0f, 0.0001f));
    CHECK(float_eq(state.arousal, -1.0f, 0.0001f));

    nb_emotion_core_apply_stimulus(&state, -5.0f, 5.0f);
    CHECK(float_eq(state.valence, -1.0f, 0.0001f));
    CHECK(float_eq(state.arousal, 1.0f, 0.0001f));
}

static void test_apply_stimulus_accumulates_within_bounds(void)
{
    nb_emotion_state_t state;

    nb_emotion_core_init(&state, 1u);
    nb_emotion_core_apply_stimulus(&state, 0.3f, 0.2f);
    CHECK(float_eq(state.valence, 0.3f, 0.0001f));
    CHECK(float_eq(state.arousal, 0.2f, 0.0001f));

    nb_emotion_core_apply_stimulus(&state, 0.3f, 0.2f);
    CHECK(float_eq(state.valence, 0.6f, 0.0001f));
    CHECK(float_eq(state.arousal, 0.4f, 0.0001f));
}

/* BEHAVIOR.md §2 original: "decaimento exponencial para (0,0) com
 * constante ~60s até <5% do pico". S3.7 completo (item 6, RFC-VIDA-V2.md
 * §7) muda o alvo do decay pro temperamento (+0.10, +0.05) em vez de
 * (0,0) -- a CONSTANTE de decaimento (tau) não mudou, só o ponto de
 * repouso. Em t=60s o decay ainda é ~5%, mas "5% do pico" agora quer
 * dizer 5% da distância até o temperamento, não até zero. */
static void test_decay_reaches_5_percent_of_gap_to_temperament_by_60s(void)
{
    nb_emotion_state_t state;

    nb_emotion_core_init(&state, 1u);
    nb_emotion_core_apply_stimulus(&state, 1.0f, -1.0f);

    for (uint32_t ms = 0; ms < 60000u; ms += 20u) {
        nb_emotion_core_tick(&state, 20u);
    }

    /* alvo + (pico - alvo) * 0.05, com folga de tolerância. */
    CHECK(state.valence >= 0.10f && state.valence <= 0.16f);
    CHECK(state.arousal >= -0.02f && state.arousal <= 0.06f);
}

static void test_decay_converges_monotonically_toward_temperament(void)
{
    nb_emotion_state_t state;
    float prev_valence;

    nb_emotion_core_init(&state, 1u);
    nb_emotion_core_apply_stimulus(&state, 0.8f, 0.0f);
    prev_valence = state.valence;

    for (int i = 0; i < 50; ++i) {
        nb_emotion_core_tick(&state, 500u);
        CHECK(state.valence <= prev_valence);
        /* nunca ultrapassa o alvo (+0.10) decaindo de um pico maior. */
        CHECK(state.valence >= 0.10f - 0.0001f);
        prev_valence = state.valence;
    }
}

static void test_decay_from_negative_converges_toward_temperament(void)
{
    nb_emotion_state_t state;

    nb_emotion_core_init(&state, 1u);
    nb_emotion_core_apply_stimulus(&state, -0.9f, -0.5f);

    for (int i = 0; i < 200; ++i) {
        nb_emotion_core_tick(&state, 100u);
        /* decaindo de negativo rumo ao temperamento (+0.10/+0.05) --
         * nunca ultrapassa o alvo nem sai do clamp [-1,1]. */
        CHECK(state.valence >= -1.0f && state.valence <= 0.10f + 0.0001f);
        CHECK(state.arousal >= -1.0f && state.arousal <= 0.05f + 0.0001f);
    }
}

static void test_nearest_expression_matches_each_anchor_exactly(void)
{
    /* Âncoras de VISUAL.md §2, na ordem do enum nb_face_expr_t. */
    static const float anchors[NB_FACE_EXPR_COUNT][2] = {
        {0.0f, 0.0f},   {0.7f, 0.3f},  {0.3f, 0.4f},  {0.0f, -0.8f}, {0.1f, 0.6f},
        {-0.3f, 0.2f},  {0.0f, 0.8f},  {-0.6f, -0.4f}, {-0.5f, 0.9f}, {-0.8f, 0.6f},
    };

    for (uint32_t i = 0; i < NB_FACE_EXPR_COUNT; ++i) {
        nb_emotion_state_t state;

        nb_emotion_core_init(&state, 1u);
        state.valence = anchors[i][0];
        state.arousal = anchors[i][1];
        CHECK(nb_emotion_core_nearest_expression(&state) == (nb_face_expr_t)i);
    }
}

static void test_nearest_expression_null_is_neutral(void)
{
    CHECK(nb_emotion_core_nearest_expression(NULL) == NB_FACE_EXPR_NEUTRAL);
}

static void test_clampf(void)
{
    CHECK(float_eq(nb_emotion_core_clampf(-2.0f, -1.0f, 1.0f), -1.0f, 0.0001f));
    CHECK(float_eq(nb_emotion_core_clampf(2.0f, -1.0f, 1.0f), 1.0f, 0.0001f));
    CHECK(float_eq(nb_emotion_core_clampf(0.4f, -1.0f, 1.0f), 0.4f, 0.0001f));
}

static void test_null_is_safe(void)
{
    nb_emotion_core_init(NULL, 1u);
    nb_emotion_core_apply_stimulus(NULL, 1.0f, 1.0f);
    nb_emotion_core_tick(NULL, 100u);
    nb_face_state_t out;
    nb_emotion_core_resolve_face(NULL, &out); /* não deve travar, escreve algo (NEUTRAL) */
    nb_emotion_core_resolve_face(NULL, NULL); /* out NULL também não trava */
    nb_emotion_core_set_circadian_offset(NULL, 0.1f);
}

/* RFC-VIDA-V2.md §9, host-test (4): "campo passa exatamente pelas âncoras
 * calibradas" -- só os 4 hubs participam (as outras 6 saem de uso,
 * decisão de 2026-07-06). Tolerância alargada (item 7): a variante
 * episódica tempera o resultado mesmo exatamente na âncora (peso 1.0) --
 * `mouth_curve` não é tocado por nenhuma variante, então continua exato;
 * `mouth_open`/`open_l` têm folga pro delta máximo de variante (±0.15). */
static void test_resolve_face_matches_each_hub_exactly(void)
{
    const nb_face_expr_t hubs[] = {NB_FACE_EXPR_NEUTRAL, NB_FACE_EXPR_HAPPY, NB_FACE_EXPR_SAD,
                                   NB_FACE_EXPR_ANGRY};
    /* Mesmas âncoras de s_anchors (emotion_core.c), só pros 4 hubs. */
    const float anchor_v[] = {0.0f, 0.7f, -0.6f, -0.8f};
    const float anchor_a[] = {0.0f, 0.3f, -0.4f, 0.6f};

    for (size_t i = 0; i < sizeof(hubs) / sizeof(hubs[0]); ++i) {
        nb_emotion_state_t state;
        nb_face_state_t out;

        nb_emotion_core_init(&state, (uint32_t)i + 1u);
        state.valence = anchor_v[i];
        state.arousal = anchor_a[i];
        nb_emotion_core_resolve_face(&state, &out);

        const nb_face_state_t *expected = nb_face_core_get_expression(hubs[i]);
        CHECK(float_eq(out.mouth_curve, expected->mouth_curve, 0.0001f));
        CHECK(float_eq(out.mouth_open, expected->mouth_open, 0.2f));
        CHECK(float_eq(out.open_l, expected->open_l, 0.2f));
    }
}

/* RFC §9(4): "contínuo entre elas" -- caminhando em passos pequenos de
 * NEUTRAL até HAPPY, a boca nunca dá um salto grande de um passo pro
 * outro. */
static void test_resolve_face_is_continuous_between_hubs(void)
{
    const int steps = 200;
    float prev_mouth_curve = 0.0f;
    int has_prev = 0;

    for (int i = 0; i <= steps; ++i) {
        const float t = (float)i / (float)steps;
        nb_emotion_state_t state;
        nb_face_state_t out;

        nb_emotion_core_init(&state, 1u);
        state.valence = 0.0f + t * 0.7f;
        state.arousal = 0.0f + t * 0.3f;
        nb_emotion_core_resolve_face(&state, &out);

        if (has_prev) {
            const float diff =
                (out.mouth_curve > prev_mouth_curve) ? out.mouth_curve - prev_mouth_curve
                                                     : prev_mouth_curve - out.mouth_curve;
            CHECK(diff < 0.05f); /* passo pequeno, sem salto */
        }
        prev_mouth_curve = out.mouth_curve;
        has_prev = 1;
    }
}

/* S3.7 completo, item 7: a variante sorteada ao entrar numa região
 * persiste enquanto o hub dominante não muda -- "dura o episódio"
 * (RFC §3.1). */
static void test_variant_persists_while_dominant_hub_unchanged(void)
{
    nb_emotion_state_t state;
    nb_face_state_t out;

    nb_emotion_core_init(&state, 42u);
    state.valence = 0.7f;
    state.arousal = 0.3f; /* HAPPY */
    nb_emotion_core_resolve_face(&state, &out);
    const nb_emotion_variant_t first_variant = state.active_variant;

    /* Tick várias vezes sem sair da região de HAPPY -- a variante não
     * pode mudar sozinha. */
    for (int i = 0; i < 50; ++i) {
        state.valence = 0.7f + (float)i * 0.0001f; /* ainda claramente HAPPY */
        nb_emotion_core_resolve_face(&state, &out);
        CHECK(state.active_variant == first_variant);
    }
}

/* S3.7 completo, item 7: trocar de hub dominante dispara um novo sorteio
 * de variante (não necessariamente diferente -- 50/50 -- mas o mecanismo
 * de resample dispara; testamos que dominant_hub acompanha a troca). */
static void test_variant_resamples_when_dominant_hub_changes(void)
{
    nb_emotion_state_t state;
    nb_face_state_t out;

    nb_emotion_core_init(&state, 7u);
    state.valence = 0.0f;
    state.arousal = 0.0f; /* NEUTRAL */
    nb_emotion_core_resolve_face(&state, &out);
    CHECK(state.dominant_hub == NB_FACE_EXPR_NEUTRAL);

    state.valence = -0.8f;
    state.arousal = 0.6f; /* ANGRY */
    nb_emotion_core_resolve_face(&state, &out);
    CHECK(state.dominant_hub == NB_FACE_EXPR_ANGRY);
}

int main(void)
{
    test_init_is_neutral();
    test_apply_stimulus_clamps_per_axis();
    test_apply_stimulus_accumulates_within_bounds();
    test_decay_reaches_5_percent_of_gap_to_temperament_by_60s();
    test_decay_converges_monotonically_toward_temperament();
    test_decay_from_negative_converges_toward_temperament();
    test_nearest_expression_matches_each_anchor_exactly();
    test_nearest_expression_null_is_neutral();
    test_clampf();
    test_null_is_safe();
    test_resolve_face_matches_each_hub_exactly();
    test_resolve_face_is_continuous_between_hubs();
    test_variant_persists_while_dominant_hub_unchanged();
    test_variant_resamples_when_dominant_hub_changes();

    if (failures == 0) {
        printf("emotion_core host_test: ok\n");
        return 0;
    }

    printf("emotion_core host_test: %d failure(s)\n", failures);
    return 1;
}
