#include "../audio_hal.h"

#include <math.h>
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

static void test_silence_is_zero(void) {
    int16_t samples[256] = {0};
    CHECK(float_eq(nb_audio_hal_rms_s16(samples, 256u), 0.0f, 0.0001f));
}

static void test_dc_equals_absolute_value(void) {
    int16_t samples[100];
    for (int i = 0; i < 100; ++i) {
        samples[i] = 5000;
    }
    CHECK(float_eq(nb_audio_hal_rms_s16(samples, 100u), 5000.0f, 0.5f));
}

/* Senoide com número inteiro de períodos (1000Hz a 16kHz -- 16 amostras
 * por período, 100 períodos): RMS analítico = amplitude/sqrt(2). */
static void test_sine_matches_analytic_rms(void) {
    static int16_t samples[1600];
    const double amplitude = 10000.0;
    const double freq_hz = 1000.0;
    const double sample_rate_hz = 16000.0;

    for (int i = 0; i < 1600; ++i) {
        const double t = (double)i / sample_rate_hz;
        samples[i] = (int16_t)(amplitude * sin(2.0 * 3.14159265358979323846 * freq_hz * t));
    }

    const float expected = (float)(amplitude / sqrt(2.0));
    const float actual = nb_audio_hal_rms_s16(samples, 1600u);
    CHECK(float_eq(actual, expected, 50.0f)); /* tolerância pro arredondamento int16 */
}

static void test_null_and_zero_count_are_safe(void) {
    int16_t samples[4] = {1, 2, 3, 4};
    CHECK(float_eq(nb_audio_hal_rms_s16(NULL, 4u), 0.0f, 0.0001f));
    CHECK(float_eq(nb_audio_hal_rms_s16(samples, 0u), 0.0f, 0.0001f));
    CHECK(float_eq(nb_audio_hal_rms_s16(NULL, 0u), 0.0f, 0.0001f));
}

int main(void) {
    test_silence_is_zero();
    test_dc_equals_absolute_value();
    test_sine_matches_analytic_rms();
    test_null_and_zero_count_are_safe();

    if (failures == 0) {
        printf("audio_hal host_test: ok\n");
        return 0;
    }

    printf("audio_hal host_test: %d failure(s)\n", failures);
    return 1;
}
