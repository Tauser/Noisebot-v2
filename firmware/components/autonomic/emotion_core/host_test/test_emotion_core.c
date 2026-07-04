#include "../emotion_core.h"

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

    nb_emotion_core_init(&state);
    CHECK(float_eq(state.valence, 0.0f, 0.0001f));
    CHECK(float_eq(state.arousal, 0.0f, 0.0001f));
    CHECK(nb_emotion_core_nearest_expression(&state) == NB_FACE_EXPR_NEUTRAL);
}

static void test_apply_stimulus_clamps_per_axis(void)
{
    nb_emotion_state_t state;

    nb_emotion_core_init(&state);
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

    nb_emotion_core_init(&state);
    nb_emotion_core_apply_stimulus(&state, 0.3f, 0.2f);
    CHECK(float_eq(state.valence, 0.3f, 0.0001f));
    CHECK(float_eq(state.arousal, 0.2f, 0.0001f));

    nb_emotion_core_apply_stimulus(&state, 0.3f, 0.2f);
    CHECK(float_eq(state.valence, 0.6f, 0.0001f));
    CHECK(float_eq(state.arousal, 0.4f, 0.0001f));
}

/* BEHAVIOR.md §2: "decaimento exponencial para (0,0) com constante ~60s
 * até <5% do pico". */
static void test_decay_reaches_5_percent_by_60s(void)
{
    nb_emotion_state_t state;

    nb_emotion_core_init(&state);
    nb_emotion_core_apply_stimulus(&state, 1.0f, -1.0f);

    for (uint32_t ms = 0; ms < 60000u; ms += 20u) {
        nb_emotion_core_tick(&state, 20u);
    }

    CHECK(state.valence >= 0.0f && state.valence <= 0.05f);
    CHECK(state.arousal <= 0.0f && state.arousal >= -0.05f);
}

static void test_decay_converges_monotonically_toward_zero(void)
{
    nb_emotion_state_t state;
    float prev_valence;

    nb_emotion_core_init(&state);
    nb_emotion_core_apply_stimulus(&state, 0.8f, 0.0f);
    prev_valence = state.valence;

    for (int i = 0; i < 50; ++i) {
        nb_emotion_core_tick(&state, 500u);
        CHECK(state.valence <= prev_valence);
        CHECK(state.valence >= 0.0f); /* nunca ultrapassa zero decaindo de positivo */
        prev_valence = state.valence;
    }
}

static void test_decay_from_negative_stays_negative_and_bounded(void)
{
    nb_emotion_state_t state;

    nb_emotion_core_init(&state);
    nb_emotion_core_apply_stimulus(&state, -0.9f, -0.5f);

    for (int i = 0; i < 200; ++i) {
        nb_emotion_core_tick(&state, 100u);
        CHECK(state.valence >= -1.0f && state.valence <= 0.0f);
        CHECK(state.arousal >= -1.0f && state.arousal <= 0.0f);
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
        nb_emotion_state_t state = {anchors[i][0], anchors[i][1]};
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
    nb_emotion_core_init(NULL);
    nb_emotion_core_apply_stimulus(NULL, 1.0f, 1.0f);
    nb_emotion_core_tick(NULL, 100u);
}

int main(void)
{
    test_init_is_neutral();
    test_apply_stimulus_clamps_per_axis();
    test_apply_stimulus_accumulates_within_bounds();
    test_decay_reaches_5_percent_by_60s();
    test_decay_converges_monotonically_toward_zero();
    test_decay_from_negative_stays_negative_and_bounded();
    test_nearest_expression_matches_each_anchor_exactly();
    test_nearest_expression_null_is_neutral();
    test_clampf();
    test_null_is_safe();

    if (failures == 0) {
        printf("emotion_core host_test: ok\n");
        return 0;
    }

    printf("emotion_core host_test: %d failure(s)\n", failures);
    return 1;
}
