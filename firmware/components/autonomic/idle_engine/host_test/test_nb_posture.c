#include "../nb_posture.h"

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
    nb_posture_t a;
    nb_posture_t b;

    nb_posture_init(&a, 42);
    nb_posture_init(&b, 42);
    CHECK(memcmp(&a, &b, sizeof(a)) == 0);

    for (int i = 0; i < 5000; ++i) {
        nb_posture_tick(&a, TICK_MS);
        nb_posture_tick(&b, TICK_MS);
        CHECK(a.roll == b.roll);
        CHECK(a.gaze_x == b.gaze_x && a.gaze_y == b.gaze_y);
        CHECK(a.asymmetry == b.asymmetry);
    }
}

static void test_seed_zero_does_not_lock_rng(void)
{
    nb_posture_t p;
    nb_posture_t reference;

    nb_posture_init(&p, 0);
    nb_posture_init(&reference, 0x9E3779B9u);
    CHECK(p.rng_state == reference.rng_state);
    CHECK(p.rng_state != 0u);
}

static void test_null_is_safe(void)
{
    nb_posture_init(NULL, 1);
    nb_posture_reset_to_center(NULL);
    nb_posture_tick(NULL, TICK_MS);

    nb_posture_t p;
    nb_posture_init(&p, 7);
    nb_posture_tick(&p, TICK_MS);
}

static void test_envelope_never_exceeded(void)
{
    nb_posture_t p;

    nb_posture_init(&p, 123);
    for (uint32_t ms = 0; ms < 1800000u; ms += TICK_MS) { /* 30 min simulados */
        nb_posture_tick(&p, TICK_MS);
        CHECK(p.roll >= -NB_POSTURE_ROLL_AMPLITUDE - 0.0001f &&
             p.roll <= NB_POSTURE_ROLL_AMPLITUDE + 0.0001f);
        CHECK(p.gaze_x >= -NB_POSTURE_GAZE_OFFSET_AMPLITUDE - 0.0001f &&
             p.gaze_x <= NB_POSTURE_GAZE_OFFSET_AMPLITUDE + 0.0001f);
        CHECK(p.gaze_y >= -NB_POSTURE_GAZE_OFFSET_AMPLITUDE - 0.0001f &&
             p.gaze_y <= NB_POSTURE_GAZE_OFFSET_AMPLITUDE + 0.0001f);
        CHECK(p.asymmetry >= -NB_POSTURE_ASYMMETRY_AMPLITUDE - 0.0001f &&
             p.asymmetry <= NB_POSTURE_ASYMMETRY_AMPLITUDE + 0.0001f);
    }
}

/* "nunca retorna à pose exata" (RFC §7): cada vez que uma nova pose de
 * HOLD é alcançada, ela difere da pose de HOLD anterior. */
static void test_never_repeats_the_exact_pose(void)
{
    nb_posture_t p;
    float prev_roll = 0.0f;
    float prev_gx = 0.0f;
    float prev_gy = 0.0f;
    float prev_asym = 0.0f;
    int hold_count = 0;

    nb_posture_init(&p, 55);
    for (uint32_t ms = TICK_MS; ms < 3600000u; ms += TICK_MS) { /* 60 min simulados */
        const nb_posture_phase_t before = p.phase;
        nb_posture_tick(&p, TICK_MS);
        if (before == NB_POSTURE_TRANSITION && p.phase == NB_POSTURE_HOLD) {
            if (hold_count > 0) {
                CHECK(p.roll != prev_roll || p.gaze_x != prev_gx || p.gaze_y != prev_gy ||
                     p.asymmetry != prev_asym);
            }
            prev_roll = p.roll;
            prev_gx = p.gaze_x;
            prev_gy = p.gaze_y;
            prev_asym = p.asymmetry;
            ++hold_count;
        }
    }
    CHECK(hold_count > 10); /* estatisticamente robusto sobre 60 min simulados */
}

static void test_reset_to_center_zeroes_everything(void)
{
    nb_posture_t p;

    nb_posture_init(&p, 7);
    for (uint32_t ms = 0; ms < 200000u; ms += TICK_MS) {
        nb_posture_tick(&p, TICK_MS);
    }
    /* Confere que a pose realmente saiu do centro antes do reset --
     * senão o teste não prova nada. */
    CHECK(p.roll != 0.0f || p.gaze_x != 0.0f || p.gaze_y != 0.0f || p.asymmetry != 0.0f);

    nb_posture_reset_to_center(&p);
    CHECK(p.roll == 0.0f);
    CHECK(p.gaze_x == 0.0f && p.gaze_y == 0.0f);
    CHECK(p.asymmetry == 0.0f);
    CHECK(p.phase == NB_POSTURE_HOLD);

    /* A deriva recomeça do centro dali -- não trava em zero pra sempre. */
    int moved = 0;
    for (uint32_t ms = 0; ms < 200000u; ms += TICK_MS) {
        nb_posture_tick(&p, TICK_MS);
        if (p.roll != 0.0f || p.gaze_x != 0.0f || p.gaze_y != 0.0f || p.asymmetry != 0.0f) {
            moved = 1;
            break;
        }
    }
    CHECK(moved);
}

int main(void)
{
    test_deterministic();
    test_seed_zero_does_not_lock_rng();
    test_null_is_safe();
    test_envelope_never_exceeded();
    test_never_repeats_the_exact_pose();
    test_reset_to_center_zeroes_everything();

    if (failures == 0) {
        printf("nb_posture host_test: ok\n");
        return 0;
    }

    printf("nb_posture host_test: %d failure(s)\n", failures);
    return 1;
}
