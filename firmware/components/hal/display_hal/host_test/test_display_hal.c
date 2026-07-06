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

static void test_rect_clamp_and_union(void)
{
    const nb_display_hal_rect_t outside = {400, 10, 5, 5};
    const nb_display_hal_rect_t clipped = nb_display_hal_rect_clamp((nb_display_hal_rect_t){
        .x = 300,
        .y = 230,
        .w = 40,
        .h = 20,
    });
    const nb_display_hal_rect_t united =
        nb_display_hal_rect_union((nb_display_hal_rect_t){10, 20, 30, 40},
                                  (nb_display_hal_rect_t){35, 10, 20, 20});

    CHECK(nb_display_hal_rect_is_empty(nb_display_hal_rect_clamp(outside)));
    CHECK(clipped.x == 300u);
    CHECK(clipped.y == 230u);
    CHECK(clipped.w == 20u);
    CHECK(clipped.h == 10u);
    CHECK(united.x == 10u);
    CHECK(united.y == 10u);
    CHECK(united.w == 45u);
    CHECK(united.h == 50u);
}

static void test_rect_align_for_flush_uses_cache_line_pixels(void)
{
    const nb_display_hal_rect_t aligned =
        nb_display_hal_rect_align_for_flush((nb_display_hal_rect_t){17, 5, 20, 7});
    const nb_display_hal_rect_t edge =
        nb_display_hal_rect_align_for_flush((nb_display_hal_rect_t){319, 0, 1, 1});

    CHECK(aligned.x == 16u);
    CHECK(aligned.y == 5u);
    CHECK(aligned.w == 32u);
    CHECK(aligned.h == 7u);
    CHECK(edge.x == 304u);
    CHECK(edge.w == 16u);
}

int main(void)
{
    test_init_sets_back_and_front_deterministically();
    test_swap_alternates_front_and_back();
    test_null_hal_is_safe();
    test_rect_clamp_and_union();
    test_rect_align_for_flush_uses_cache_line_pixels();

    if (failures == 0) {
        printf("display_hal host_test: ok\n");
        return 0;
    }

    printf("display_hal host_test: %d failure(s)\n", failures);
    return 1;
}
