#include "../renderer.h"

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

static int float_eq(float a, float b)
{
    const float diff = (a > b) ? (a - b) : (b - a);
    return diff < 0.0001f;
}

static void test_all_expressions_resolve_and_neutral_is_symmetric(void)
{
    for (uint32_t i = 0; i < NB_FACE_EXPR_COUNT; ++i) {
        const nb_face_state_t *state = nb_face_core_get_expression((nb_face_expr_t)i);
        CHECK(state != NULL);
        CHECK(state->open_l >= 0.0f && state->open_l <= 1.0f);
        CHECK(state->open_r >= 0.0f && state->open_r <= 1.0f);
    }

    const nb_face_state_t *neutral = nb_face_core_get_expression(NB_FACE_EXPR_NEUTRAL);
    CHECK(float_eq(neutral->open_l, neutral->open_r));
    CHECK(float_eq(neutral->y_l, neutral->y_r));
    CHECK(float_eq(neutral->tl_l, 0.0f) && float_eq(neutral->tr_l, 0.0f));
}

static void test_unknown_expression_falls_back_to_neutral(void)
{
    const nb_face_state_t *fallback = nb_face_core_get_expression((nb_face_expr_t)999);
    const nb_face_state_t *neutral = nb_face_core_get_expression(NB_FACE_EXPR_NEUTRAL);
    CHECK(fallback == neutral);
}

static void test_lerp_endpoints_and_midpoint(void)
{
    const nb_face_state_t *neutral = nb_face_core_get_expression(NB_FACE_EXPR_NEUTRAL);
    const nb_face_state_t *happy = nb_face_core_get_expression(NB_FACE_EXPR_HAPPY);
    nb_face_state_t out;

    nb_face_core_lerp(neutral, happy, 0.0f, &out);
    CHECK(float_eq(out.open_l, neutral->open_l));
    CHECK(float_eq(out.bl_l, neutral->bl_l));

    nb_face_core_lerp(neutral, happy, 1.0f, &out);
    CHECK(float_eq(out.open_l, happy->open_l));
    CHECK(float_eq(out.bl_l, happy->bl_l));

    nb_face_core_lerp(neutral, happy, 0.5f, &out);
    CHECK(float_eq(out.open_l, (neutral->open_l + happy->open_l) / 2.0f));
    CHECK(float_eq(out.bl_l, (neutral->bl_l + happy->bl_l) / 2.0f));
}

static void test_clampf(void)
{
    CHECK(float_eq(nb_face_core_clampf(-2.0f, -1.0f, 1.0f), -1.0f));
    CHECK(float_eq(nb_face_core_clampf(2.0f, -1.0f, 1.0f), 1.0f));
    CHECK(float_eq(nb_face_core_clampf(0.3f, -1.0f, 1.0f), 0.3f));
}

static void test_eye_column_closed_when_open_near_zero(void)
{
    nb_face_eye_column_t col;

    nb_face_core_eye_column(0.0f, 0, 0, 0, 0, 0, .64f, .64f, 0, 0, NB_FACE_EYE_HALF_WIDTH, 120.0f,
                            NB_FACE_EYE_HALF_WIDTH, &col);
    CHECK(col.eye_closed);
}

static void test_eye_column_open_neutral_is_symmetric_and_centered(void)
{
    const nb_face_state_t *neutral = nb_face_core_get_expression(NB_FACE_EXPR_NEUTRAL);
    nb_face_eye_column_t left_edge;
    nb_face_eye_column_t center;
    nb_face_eye_column_t right_edge;
    const float cy = 122.0f;

    nb_face_core_eye_column(neutral->open_l, neutral->tl_l, neutral->tr_l, neutral->bl_l,
                            neutral->br_l, neutral->squint_l, neutral->round_top,
                            neutral->round_bottom, neutral->curve_top, neutral->curve_bottom,
                            NB_FACE_EYE_HALF_WIDTH, cy, 0, &left_edge);
    nb_face_core_eye_column(neutral->open_l, neutral->tl_l, neutral->tr_l, neutral->bl_l,
                            neutral->br_l, neutral->squint_l, neutral->round_top,
                            neutral->round_bottom, neutral->curve_top, neutral->curve_bottom,
                            NB_FACE_EYE_HALF_WIDTH, cy, NB_FACE_EYE_HALF_WIDTH, &center);
    nb_face_core_eye_column(neutral->open_l, neutral->tl_l, neutral->tr_l, neutral->bl_l,
                            neutral->br_l, neutral->squint_l, neutral->round_top,
                            neutral->round_bottom, neutral->curve_top, neutral->curve_bottom,
                            NB_FACE_EYE_HALF_WIDTH, cy, 2 * NB_FACE_EYE_HALF_WIDTH, &right_edge);

    CHECK(!center.eye_closed);
    /* Simétrico: NEUTRAL tem os quatro cantos iguais -- bordas esquerda e
     * direita da mesma coluna relativa devem coincidir. */
    CHECK(left_edge.top_full == right_edge.top_full);
    CHECK(left_edge.bottom_full == right_edge.bottom_full);
    /* Olho aberto centrado em cy: span cheio (sem squint/curvatura). */
    CHECK(center.top_full < (int16_t)cy);
    CHECK(center.bottom_full > (int16_t)cy);
}

static void test_eye_column_squint_can_close_locally(void)
{
    nb_face_eye_column_t col;

    /* squint extremo fecha o olho (span vazio) mesmo com open=1: squint_px
     * cresce com squint*hh, então um valor bem acima do 1.0 usado nas
     * expressões reais empurra "top" além de "bottom", fechando a coluna. */
    nb_face_core_eye_column(1.0f, 0, 0, 0, 0, 10.0f, .64f, .64f, 0, 0, NB_FACE_EYE_HALF_WIDTH,
                            120.0f, NB_FACE_EYE_HALF_WIDTH, &col);
    CHECK(!col.eye_closed);
    CHECK(col.bottom_full < col.top_full);
}

/* Boca (S3.7 completo, item 5, RFC-VIDA-V2.md §3.1): só os 4 hubs têm
 * boca não-neutra; as outras 6 âncoras continuam "boca neutra" (0,0),
 * intocadas conforme o RFC decidiu pra esta fase. */
static void test_mouth_only_four_hubs_are_non_neutral(void)
{
    const nb_face_expr_t hubs[] = {NB_FACE_EXPR_NEUTRAL, NB_FACE_EXPR_HAPPY, NB_FACE_EXPR_SAD,
                                   NB_FACE_EXPR_ANGRY};
    const nb_face_expr_t non_hubs[] = {NB_FACE_EXPR_CURIOUS,   NB_FACE_EXPR_SLEEPY,
                                       NB_FACE_EXPR_FOCUSED,   NB_FACE_EXPR_SUSPICIOUS,
                                       NB_FACE_EXPR_SURPRISED, NB_FACE_EXPR_ALARMED};

    for (size_t i = 0; i < sizeof(non_hubs) / sizeof(non_hubs[0]); ++i) {
        const nb_face_state_t *s = nb_face_core_get_expression(non_hubs[i]);
        CHECK(float_eq(s->mouth_open, 0.0f));
        CHECK(float_eq(s->mouth_curve, 0.0f));
    }

    const nb_face_state_t *neutral = nb_face_core_get_expression(NB_FACE_EXPR_NEUTRAL);
    const nb_face_state_t *happy = nb_face_core_get_expression(NB_FACE_EXPR_HAPPY);
    const nb_face_state_t *sad = nb_face_core_get_expression(NB_FACE_EXPR_SAD);
    const nb_face_state_t *angry = nb_face_core_get_expression(NB_FACE_EXPR_ANGRY);

    /* "fechada, micro-curva": NEUTRAL tem um sorriso bem sutil, não zero. */
    CHECK(neutral->mouth_curve > 0.0f && neutral->mouth_curve < 0.2f);
    /* "sorriso (aberto no nível alto)": HAPPY abre e curva positivo. */
    CHECK(happy->mouth_curve > 0.3f);
    CHECK(happy->mouth_open > 0.0f);
    /* "curva pra baixo suave" / "curva invertida firme": ambos negativos,
     * ANGRY mais acentuado que SAD ("firme" vs "suave"). */
    CHECK(sad->mouth_curve < 0.0f);
    CHECK(angry->mouth_curve < sad->mouth_curve);
    (void)hubs;
}

/* RFC-VIDA-V2.md §3.1a (emenda 2026-07-07): boca é canal de intensidade,
 * não traço permanente -- mouth_open<=0 (quem decide isso é o gating de
 * intensidade do emotion_core) precisa dar ZERO pixel, sem piso de
 * meia-altura mínima. */
static void test_mouth_column_is_absent_when_open_is_zero(void)
{
    nb_face_mouth_column_t col;

    nb_face_core_mouth_column(0.0f, -0.7f, NB_FACE_MOUTH_HALF_WIDTH, 190.0f, 0, &col);
    CHECK(col.mouth_absent);

    /* curvatura sozinha (sem abertura) não basta pra aparecer -- mesmo um
     * franzido forte fica de fora quando mouth_open é 0. */
    nb_face_core_mouth_column(0.0f, 1.0f, NB_FACE_MOUTH_HALF_WIDTH, 190.0f,
                              NB_FACE_MOUTH_HALF_WIDTH, &col);
    CHECK(col.mouth_absent);
}

static void test_mouth_column_present_when_open_is_positive(void)
{
    nb_face_mouth_column_t col;

    nb_face_core_mouth_column(0.5f, 0.0f, NB_FACE_MOUTH_HALF_WIDTH, 190.0f, 0, &col);
    CHECK(!col.mouth_absent);
    CHECK(col.bottom_full >= col.top_full);
}

static void test_mouth_column_smile_curves_up_at_edges(void)
{
    const nb_face_state_t *happy = nb_face_core_get_expression(NB_FACE_EXPR_HAPPY);
    nb_face_mouth_column_t center;
    nb_face_mouth_column_t edge;
    const float cy = 190.0f;

    nb_face_core_mouth_column(happy->mouth_open, happy->mouth_curve, NB_FACE_MOUTH_HALF_WIDTH, cy,
                              NB_FACE_MOUTH_HALF_WIDTH, &center);
    nb_face_core_mouth_column(happy->mouth_open, happy->mouth_curve, NB_FACE_MOUTH_HALF_WIDTH, cy,
                              0, &edge);

    /* Sorriso: pontas sobem em relação ao centro -- top_full da ponta é
     * menor (mais alto na tela) que o do centro. */
    CHECK(edge.top_full < center.top_full);
}

static void test_mouth_lerp_interpolates(void)
{
    const nb_face_state_t *neutral = nb_face_core_get_expression(NB_FACE_EXPR_NEUTRAL);
    const nb_face_state_t *happy = nb_face_core_get_expression(NB_FACE_EXPR_HAPPY);
    nb_face_state_t out;

    nb_face_core_lerp(neutral, happy, 0.5f, &out);
    CHECK(float_eq(out.mouth_open, (neutral->mouth_open + happy->mouth_open) / 2.0f));
    CHECK(float_eq(out.mouth_curve, (neutral->mouth_curve + happy->mouth_curve) / 2.0f));
}

int main(void)
{
    test_all_expressions_resolve_and_neutral_is_symmetric();
    test_unknown_expression_falls_back_to_neutral();
    test_lerp_endpoints_and_midpoint();
    test_clampf();
    test_eye_column_closed_when_open_near_zero();
    test_eye_column_open_neutral_is_symmetric_and_centered();
    test_eye_column_squint_can_close_locally();
    test_mouth_only_four_hubs_are_non_neutral();
    test_mouth_column_is_absent_when_open_is_zero();
    test_mouth_column_present_when_open_is_positive();
    test_mouth_column_smile_curves_up_at_edges();
    test_mouth_lerp_interpolates();

    if (failures == 0) {
        printf("face_renderer host_test: ok\n");
        return 0;
    }

    printf("face_renderer host_test: %d failure(s)\n", failures);
    return 1;
}
