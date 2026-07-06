#include "../nb_breath.h"

#include <stdio.h>

static int failures;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);           \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static void test_zero_period_is_neutral(void)
{
    CHECK(nb_breath_scale(0u, 0u, 0.02f) == 1.0f);
    CHECK(nb_breath_scale(12345u, 0u, 0.02f) == 1.0f);
}

static void test_deterministic(void)
{
    for (uint32_t ms = 0u; ms < 20000u; ms += 33u) {
        const float a = nb_breath_scale(ms, NB_BREATH_PERIOD_MS_DEFAULT, NB_BREATH_AMPLITUDE_DEFAULT);
        const float b = nb_breath_scale(ms, NB_BREATH_PERIOD_MS_DEFAULT, NB_BREATH_AMPLITUDE_DEFAULT);
        CHECK(a == b);
    }
}

static void test_never_exceeds_amplitude_envelope(void)
{
    const float amp = NB_BREATH_AMPLITUDE_DEFAULT;

    for (uint32_t ms = 0u; ms < 60000u; ms += 20u) {
        const float scale = nb_breath_scale(ms, NB_BREATH_PERIOD_MS_DEFAULT, amp);
        CHECK(scale >= 1.0f - amp - 0.0001f);
        CHECK(scale <= 1.0f + amp + 0.0001f);
    }
}

static void test_periodic(void)
{
    /* Mesmo instante módulo o período -> mesmo fator. */
    const uint32_t period = NB_BREATH_PERIOD_MS_DEFAULT;
    const float amp = NB_BREATH_AMPLITUDE_DEFAULT;

    for (uint32_t phase_ms = 0u; phase_ms < period; phase_ms += 500u) {
        const float a = nb_breath_scale(phase_ms, period, amp);
        const float b = nb_breath_scale(phase_ms + period, period, amp);
        const float b2 = nb_breath_scale(phase_ms + 3u * period, period, amp);
        CHECK(a == b);
        CHECK(a == b2);
    }
}

static void test_starts_at_neutral(void)
{
    /* t=0 -> sin(0)=0 -> fator 1.0 (nem inspirando nem expirando). */
    CHECK(nb_breath_scale(0u, NB_BREATH_PERIOD_MS_DEFAULT, NB_BREATH_AMPLITUDE_DEFAULT) == 1.0f);
}

int main(void)
{
    test_zero_period_is_neutral();
    test_deterministic();
    test_never_exceeds_amplitude_envelope();
    test_periodic();
    test_starts_at_neutral();

    if (failures == 0) {
        printf("nb_breath host_test: ok\n");
        return 0;
    }

    printf("nb_breath host_test: %d failure(s)\n", failures);
    return 1;
}
