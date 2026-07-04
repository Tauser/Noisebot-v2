#include "../touch_hal.h"

#include <stdio.h>

static int failures;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);           \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static void test_average_of_known_samples(void)
{
    const uint32_t samples[] = {100, 200, 300};
    CHECK(nb_touch_hal_compute_baseline(samples, 3) == 200);
}

static void test_single_sample(void)
{
    const uint32_t samples[] = {4242};
    CHECK(nb_touch_hal_compute_baseline(samples, 1) == 4242);
}

static void test_large_values_do_not_overflow(void)
{
    const uint32_t samples[] = {0xFFFFFFFFu, 0xFFFFFFFFu};
    CHECK(nb_touch_hal_compute_baseline(samples, 2) == 0xFFFFFFFFu);
}

static void test_empty_or_null_is_safe(void)
{
    const uint32_t samples[] = {1, 2, 3};
    CHECK(nb_touch_hal_compute_baseline(samples, 0) == 0);
    CHECK(nb_touch_hal_compute_baseline(NULL, 3) == 0);
    CHECK(nb_touch_hal_compute_baseline(NULL, 0) == 0);
}

int main(void)
{
    test_average_of_known_samples();
    test_single_sample();
    test_large_values_do_not_overflow();
    test_empty_or_null_is_safe();

    if (failures == 0) {
        printf("touch_hal host_test: ok\n");
        return 0;
    }

    printf("touch_hal host_test: %d failure(s)\n", failures);
    return 1;
}
