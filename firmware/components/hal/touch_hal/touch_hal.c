#include "touch_hal.h"

uint32_t nb_touch_hal_compute_baseline(const uint32_t *samples, size_t count)
{
    if (samples == NULL || count == 0) {
        return 0;
    }

    uint64_t sum = 0;
    for (size_t i = 0; i < count; ++i) {
        sum += samples[i];
    }
    return (uint32_t)(sum / (uint64_t)count);
}
