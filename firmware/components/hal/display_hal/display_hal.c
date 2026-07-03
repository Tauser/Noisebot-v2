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
