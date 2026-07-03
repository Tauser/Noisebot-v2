#include "../display_hal.h"

#include <stdio.h>

static int failures;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);           \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static void test_init_sets_back_and_front_deterministically(void)
{
    nb_display_hal_t hal;
    uint16_t buf_a[4];
    uint16_t buf_b[4];

    nb_display_hal_init(&hal, buf_a, buf_b);
    CHECK(nb_display_hal_get_front_buffer(&hal) == buf_a);
    CHECK(nb_display_hal_get_back_buffer(&hal) == buf_b);
}

static void test_swap_alternates_front_and_back(void)
{
    nb_display_hal_t hal;
    uint16_t buf_a[4];
    uint16_t buf_b[4];

    nb_display_hal_init(&hal, buf_a, buf_b);

    nb_display_hal_swap(&hal);
    CHECK(nb_display_hal_get_front_buffer(&hal) == buf_b);
    CHECK(nb_display_hal_get_back_buffer(&hal) == buf_a);

    nb_display_hal_swap(&hal);
    CHECK(nb_display_hal_get_front_buffer(&hal) == buf_a);
    CHECK(nb_display_hal_get_back_buffer(&hal) == buf_b);
}

static void test_null_hal_is_safe(void)
{
    CHECK(nb_display_hal_get_front_buffer(NULL) == NULL);
    CHECK(nb_display_hal_get_back_buffer(NULL) == NULL);
    nb_display_hal_swap(NULL);
    nb_display_hal_init(NULL, NULL, NULL);
}

int main(void)
{
    test_init_sets_back_and_front_deterministically();
    test_swap_alternates_front_and_back();
    test_null_hal_is_safe();

    if (failures == 0) {
        printf("display_hal host_test: ok\n");
        return 0;
    }

    printf("display_hal host_test: %d failure(s)\n", failures);
    return 1;
}
