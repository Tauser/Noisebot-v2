#include "../circadian_core.h"

#include <stdio.h>

static int failures;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);           \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static int float_eq(float a, float b, float tol) {
    const float diff = (a > b) ? (a - b) : (b - a);
    return diff < tol;
}

static uint64_t unix_ms_at_hour(uint32_t hour, uint32_t minute) {
    return ((uint64_t)hour * 3600u + (uint64_t)minute * 60u) * 1000u;
}

static void test_phase_for_hour_boundaries(void) {
    CHECK(nb_circadian_core_phase_for_hour(21) == NB_CIRCADIAN_PHASE_DUSK);
    CHECK(nb_circadian_core_phase_for_hour(22) == NB_CIRCADIAN_PHASE_NIGHT);
    CHECK(nb_circadian_core_phase_for_hour(5) == NB_CIRCADIAN_PHASE_NIGHT);
    CHECK(nb_circadian_core_phase_for_hour(6) == NB_CIRCADIAN_PHASE_DAWN);
    CHECK(nb_circadian_core_phase_for_hour(7) == NB_CIRCADIAN_PHASE_DAWN);
    CHECK(nb_circadian_core_phase_for_hour(8) == NB_CIRCADIAN_PHASE_DAY);
    CHECK(nb_circadian_core_phase_for_hour(19) == NB_CIRCADIAN_PHASE_DAY);
    CHECK(nb_circadian_core_phase_for_hour(20) == NB_CIRCADIAN_PHASE_DUSK);
    CHECK(nb_circadian_core_phase_for_hour(32) == nb_circadian_core_phase_for_hour(8)); /* módulo 24 */
}

static void test_no_time_source_defaults_neutral(void) {
    nb_circadian_core_t core;
    nb_circadian_core_init(&core);

    const nb_circadian_output_t out = nb_circadian_core_tick(&core, 16u);
    CHECK(!out.has_time_source);
    CHECK(out.phase == NB_CIRCADIAN_PHASE_DAY);
    CHECK(float_eq(out.brightness_scale, 1.0f, 0.0001f));
    CHECK(!out.quiet_mode);
}

static void test_day_is_full_brightness_no_quiet(void) {
    nb_circadian_core_t core;
    nb_circadian_core_init(&core);
    nb_circadian_core_set_time_anchor(&core, unix_ms_at_hour(12, 0));

    const nb_circadian_output_t out = nb_circadian_core_tick(&core, 0u);
    CHECK(out.has_time_source);
    CHECK(out.phase == NB_CIRCADIAN_PHASE_DAY);
    CHECK(float_eq(out.brightness_scale, 1.0f, 0.0001f));
    CHECK(!out.quiet_mode);
}

static void test_night_is_floor_brightness_with_quiet(void) {
    nb_circadian_core_t core;
    nb_circadian_core_init(&core);
    nb_circadian_core_set_time_anchor(&core, unix_ms_at_hour(2, 0));

    const nb_circadian_output_t out = nb_circadian_core_tick(&core, 0u);
    CHECK(out.phase == NB_CIRCADIAN_PHASE_NIGHT);
    CHECK(float_eq(out.brightness_scale, 0.05f, 0.0001f));
    CHECK(out.quiet_mode);
}

static void test_dawn_ramps_up_monotonically(void) {
    nb_circadian_core_t core;
    nb_circadian_core_init(&core);
    nb_circadian_core_set_time_anchor(&core, unix_ms_at_hour(6, 0));

    float prev = -1.0f;
    for (uint32_t minute = 0; minute <= 120; minute += 10) {
        const nb_circadian_output_t out = nb_circadian_core_tick(&core, (minute == 0) ? 0u : 10u * 60u * 1000u);
        CHECK(out.phase == NB_CIRCADIAN_PHASE_DAWN || (minute == 120 && out.phase == NB_CIRCADIAN_PHASE_DAY));
        CHECK(out.brightness_scale >= prev);
        prev = out.brightness_scale;
    }
    CHECK(float_eq(prev, 1.0f, 0.0001f)); /* termina em DAY, brilho pleno */
}

static void test_dusk_ramps_down_monotonically(void) {
    nb_circadian_core_t core;
    nb_circadian_core_init(&core);
    nb_circadian_core_set_time_anchor(&core, unix_ms_at_hour(20, 0));

    float prev = 2.0f;
    for (uint32_t minute = 0; minute <= 120; minute += 10) {
        const nb_circadian_output_t out = nb_circadian_core_tick(&core, (minute == 0) ? 0u : 10u * 60u * 1000u);
        CHECK(out.brightness_scale <= prev);
        prev = out.brightness_scale;
    }
    CHECK(float_eq(prev, 0.05f, 0.0001f)); /* termina em NIGHT, piso */
}

static void test_set_time_anchor_recalibrates(void) {
    nb_circadian_core_t core;
    nb_circadian_core_init(&core);
    nb_circadian_core_set_time_anchor(&core, unix_ms_at_hour(12, 0));
    nb_circadian_core_tick(&core, 5000u); /* avança um pouco no relógio interno */

    /* Recalibra pra outro horário -- a nova âncora vale a partir de agora,
     * não soma ao deslocamento anterior. */
    nb_circadian_core_set_time_anchor(&core, unix_ms_at_hour(2, 0));
    const nb_circadian_output_t out = nb_circadian_core_tick(&core, 0u);
    CHECK(out.phase == NB_CIRCADIAN_PHASE_NIGHT);
}

static void test_now_unix_ms_advances_with_monotonic(void) {
    nb_circadian_core_t core;
    nb_circadian_core_init(&core);
    const uint64_t anchor = unix_ms_at_hour(10, 0);
    nb_circadian_core_set_time_anchor(&core, anchor);

    nb_circadian_core_tick(&core, 60000u); /* 60s */
    CHECK(nb_circadian_core_now_unix_ms(&core) == anchor + 60000u);
}

static void test_now_unix_ms_without_anchor_is_zero(void) {
    nb_circadian_core_t core;
    nb_circadian_core_init(&core);
    CHECK(nb_circadian_core_now_unix_ms(&core) == 0u);
}

static void test_null_is_safe(void) {
    nb_circadian_core_init(NULL);
    nb_circadian_core_set_time_anchor(NULL, 12345u);
    const nb_circadian_output_t out = nb_circadian_core_tick(NULL, 16u);
    CHECK(!out.has_time_source);
    CHECK(nb_circadian_core_now_unix_ms(NULL) == 0u);
}

int main(void) {
    test_phase_for_hour_boundaries();
    test_no_time_source_defaults_neutral();
    test_day_is_full_brightness_no_quiet();
    test_night_is_floor_brightness_with_quiet();
    test_dawn_ramps_up_monotonically();
    test_dusk_ramps_down_monotonically();
    test_set_time_anchor_recalibrates();
    test_now_unix_ms_advances_with_monotonic();
    test_now_unix_ms_without_anchor_is_zero();
    test_null_is_safe();

    if (failures == 0) {
        printf("circadian_core host_test: ok\n");
        return 0;
    }

    printf("circadian_core host_test: %d failure(s)\n", failures);
    return 1;
}
