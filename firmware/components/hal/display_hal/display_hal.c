#include "display_hal.h"

#include <stddef.h>

void nb_display_hal_init(nb_display_hal_t *hal, uint16_t *buffer_a, uint16_t *buffer_b)
{
    if (hal == NULL) {
        return;
    }

    hal->buffers[0] = buffer_a;
    hal->buffers[1] = buffer_b;
    hal->front_index = 0u;
}

uint16_t *nb_display_hal_get_back_buffer(const nb_display_hal_t *hal)
{
    if (hal == NULL) {
        return NULL;
    }

    return hal->buffers[1u - hal->front_index];
}

uint16_t *nb_display_hal_get_front_buffer(const nb_display_hal_t *hal)
{
    if (hal == NULL) {
        return NULL;
    }

    return hal->buffers[hal->front_index];
}

void nb_display_hal_swap(nb_display_hal_t *hal)
{
    if (hal == NULL) {
        return;
    }

    hal->front_index = 1u - hal->front_index;
}

bool nb_display_hal_rect_is_empty(nb_display_hal_rect_t rect)
{
    return rect.w == 0u || rect.h == 0u;
}

nb_display_hal_rect_t nb_display_hal_rect_clamp(nb_display_hal_rect_t rect)
{
    const uint32_t max_x = NB_DISPLAY_HAL_WIDTH;
    const uint32_t max_y = NB_DISPLAY_HAL_HEIGHT;
    uint32_t x0 = rect.x;
    uint32_t y0 = rect.y;
    uint32_t x1 = x0 + rect.w;
    uint32_t y1 = y0 + rect.h;

    if (x0 >= max_x || y0 >= max_y) {
        return NB_DISPLAY_HAL_RECT_EMPTY;
    }
    if (x1 > max_x) {
        x1 = max_x;
    }
    if (y1 > max_y) {
        y1 = max_y;
    }
    if (x1 <= x0 || y1 <= y0) {
        return NB_DISPLAY_HAL_RECT_EMPTY;
    }

    return (nb_display_hal_rect_t){
        .x = (uint16_t)x0,
        .y = (uint16_t)y0,
        .w = (uint16_t)(x1 - x0),
        .h = (uint16_t)(y1 - y0),
    };
}

nb_display_hal_rect_t nb_display_hal_rect_union(nb_display_hal_rect_t a,
                                                nb_display_hal_rect_t b)
{
    a = nb_display_hal_rect_clamp(a);
    b = nb_display_hal_rect_clamp(b);
    if (nb_display_hal_rect_is_empty(a)) {
        return b;
    }
    if (nb_display_hal_rect_is_empty(b)) {
        return a;
    }

    const uint16_t x0 = (a.x < b.x) ? a.x : b.x;
    const uint16_t y0 = (a.y < b.y) ? a.y : b.y;
    const uint16_t ax1 = (uint16_t)(a.x + a.w);
    const uint16_t ay1 = (uint16_t)(a.y + a.h);
    const uint16_t bx1 = (uint16_t)(b.x + b.w);
    const uint16_t by1 = (uint16_t)(b.y + b.h);
    const uint16_t x1 = (ax1 > bx1) ? ax1 : bx1;
    const uint16_t y1 = (ay1 > by1) ? ay1 : by1;

    return (nb_display_hal_rect_t){
        .x = x0,
        .y = y0,
        .w = (uint16_t)(x1 - x0),
        .h = (uint16_t)(y1 - y0),
    };
}

nb_display_hal_rect_t nb_display_hal_rect_align_for_flush(nb_display_hal_rect_t rect)
{
    rect = nb_display_hal_rect_clamp(rect);
    if (nb_display_hal_rect_is_empty(rect)) {
        return rect;
    }

    const uint32_t align = NB_DISPLAY_HAL_FLUSH_ALIGN_PIXELS;
    uint32_t x0 = rect.x;
    uint32_t x1 = (uint32_t)rect.x + rect.w;

    x0 = (x0 / align) * align;
    x1 = ((x1 + align - 1u) / align) * align;
    if (x1 > NB_DISPLAY_HAL_WIDTH) {
        x1 = NB_DISPLAY_HAL_WIDTH;
    }

    return (nb_display_hal_rect_t){
        .x = (uint16_t)x0,
        .y = rect.y,
        .w = (uint16_t)(x1 - x0),
        .h = rect.h,
    };
}
