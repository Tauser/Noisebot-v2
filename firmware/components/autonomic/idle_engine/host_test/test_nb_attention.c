#include "../nb_attention.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);           \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

#define TICK_MS 20u

static void test_deterministic(void)
{
    nb_attention_t a;
    nb_attention_t b;

    nb_attention_init(&a, 42);
    nb_attention_init(&b, 42);
    CHECK(memcmp(&a, &b, sizeof(a)) == 0);

    for (int i = 0; i < 5000; ++i) {
        float ax;
        float ay;
        float bx;
        float by;

        nb_attention_tick(&a, TICK_MS, &ax, &ay);
        nb_attention_tick(&b, TICK_MS, &bx, &by);
        CHECK(ax == bx);
        CHECK(ay == by);
    }
}

static void test_seed_zero_does_not_lock_rng(void)
{
    nb_attention_t att;
    nb_attention_t reference;

    nb_attention_init(&att, 0);
    nb_attention_init(&reference, 0x9E3779B9u);
    CHECK(att.rng_state == reference.rng_state);
    CHECK(att.rng_state != 0u);
}

static void test_null_is_safe(void)
{
    nb_attention_init(NULL, 1);
    nb_attention_set_saccade_callback(NULL, NULL, NULL);
    nb_attention_tick(NULL, TICK_MS, NULL, NULL);

    nb_attention_t att;
    nb_attention_init(&att, 7);
    nb_attention_tick(&att, TICK_MS, NULL, NULL); /* saídas NULL com struct válida */
}

static void test_gaze_never_leaves_envelope(void)
{
    nb_attention_t att;

    nb_attention_init(&att, 123);
    for (uint32_t ms = 0; ms < 300000u; ms += TICK_MS) {
        float gx;
        float gy;

        nb_attention_tick(&att, TICK_MS, &gx, &gy);
        CHECK(gx >= -NB_ATTENTION_ENVELOPE - 0.0001f && gx <= NB_ATTENTION_ENVELOPE + 0.0001f);
        CHECK(gy >= -NB_ATTENTION_ENVELOPE - 0.0001f && gy <= NB_ATTENTION_ENVELOPE + 0.0001f);
    }
}

/* Percorre fixação e sacada registrando a duração de cada fixação
 * (intervalo entre sacadas consecutivas) e checa a faixa 500-3000ms do
 * RFC §7. */
static void test_fixation_duration_in_range(void)
{
    nb_attention_t att;
    uint32_t saccade_count = 0;
    uint32_t last_saccade_end_ms = 0;
    int saw_any_fixation = 0;

    nb_attention_init(&att, 55);
    for (uint32_t ms = TICK_MS; ms < 600000u; ms += TICK_MS) {
        const nb_attention_phase_t before = att.phase;
        nb_attention_tick(&att, TICK_MS, NULL, NULL);
        if (before == NB_ATTENTION_SACCADE && att.phase == NB_ATTENTION_FIXATE) {
            /* acabou de entrar em FIXATE -- next fixation's duration is
             * att.phase_duration_ms, sampled inside the tick that flipped it */
            CHECK(att.phase_duration_ms >= NB_ATTENTION_FIXATION_MIN_MS);
            CHECK(att.phase_duration_ms <= NB_ATTENTION_FIXATION_MAX_MS);
            saw_any_fixation = 1;
            ++saccade_count;
            last_saccade_end_ms = ms;
        }
    }
    (void)last_saccade_end_ms;
    CHECK(saw_any_fixation);
    CHECK(saccade_count > 10); /* estatisticamente robusto sobre 10 min simulados */
}

static void test_tremor_bounded_during_fixation(void)
{
    nb_attention_t att;

    nb_attention_init(&att, 7);
    for (uint32_t ms = 0; ms < 300000u; ms += TICK_MS) {
        float gx;
        float gy;

        nb_attention_tick(&att, TICK_MS, &gx, &gy);
        if (att.phase == NB_ATTENTION_FIXATE) {
            const float dx = gx - att.target_x;
            const float dy = gy - att.target_y;
            CHECK(dx >= -NB_ATTENTION_TREMOR_AMPLITUDE - 0.0001f &&
                 dx <= NB_ATTENTION_TREMOR_AMPLITUDE + 0.0001f);
            CHECK(dy >= -NB_ATTENTION_TREMOR_AMPLITUDE - 0.0001f &&
                 dy <= NB_ATTENTION_TREMOR_AMPLITUDE + 0.0001f);
        }
    }
}

/* ~40% dos alvos de sacada devem voltar perto do centro (viés "olhar pro
 * usuário", RFC §7) -- estatístico, sobre muitas sacadas. */
static void test_center_bias_roughly_forty_percent(void)
{
    nb_attention_t att;
    uint32_t total_targets = 0;
    uint32_t centered_targets = 0;

    nb_attention_init(&att, 999);
    for (uint32_t ms = TICK_MS; ms < 1800000u; ms += TICK_MS) { /* 30 min simulados */
        const nb_attention_phase_t before = att.phase;
        nb_attention_tick(&att, TICK_MS, NULL, NULL);
        if (before == NB_ATTENTION_FIXATE && att.phase == NB_ATTENTION_SACCADE) {
            ++total_targets;
            if (att.target_x >= -NB_ATTENTION_CENTER_BIAS_RADIUS - 0.0001f &&
                att.target_x <= NB_ATTENTION_CENTER_BIAS_RADIUS + 0.0001f &&
                att.target_y >= -NB_ATTENTION_CENTER_BIAS_RADIUS - 0.0001f &&
                att.target_y <= NB_ATTENTION_CENTER_BIAS_RADIUS + 0.0001f) {
                ++centered_targets;
            }
        }
    }

    CHECK(total_targets > 50);
    const float ratio = (float)centered_targets / (float)total_targets;
    /* tolerância generosa (30-50%) -- é estatística sobre RNG determinístico,
     * não precisa bater em 40.0% exato. */
    CHECK(ratio >= 0.30f && ratio <= 0.50f);
}

static int saccade_fired;

static void on_saccade_cb(void *ctx)
{
    int *flag = (int *)ctx;
    *flag = 1;
}

static void test_saccade_callback_fires(void)
{
    nb_attention_t att;

    saccade_fired = 0;
    nb_attention_init(&att, 321);
    nb_attention_set_saccade_callback(&att, on_saccade_cb, &saccade_fired);

    for (uint32_t ms = 0; ms < 10000u && !saccade_fired; ms += TICK_MS) {
        nb_attention_tick(&att, TICK_MS, NULL, NULL);
    }
    CHECK(saccade_fired);
}

int main(void)
{
    test_deterministic();
    test_seed_zero_does_not_lock_rng();
    test_null_is_safe();
    test_gaze_never_leaves_envelope();
    test_fixation_duration_in_range();
    test_tremor_bounded_during_fixation();
    test_center_bias_roughly_forty_percent();
    test_saccade_callback_fires();

    if (failures == 0) {
        printf("nb_attention host_test: ok\n");
        return 0;
    }

    printf("nb_attention host_test: %d failure(s)\n", failures);
    return 1;
}
