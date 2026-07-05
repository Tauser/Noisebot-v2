#include "audio_hal.h"

#include <math.h>

float nb_audio_hal_rms_s16(const int16_t *samples, size_t count) {
    if (!samples || count == 0u) {
        return 0.0f;
    }

    uint64_t sum_sq = 0u;
    for (size_t i = 0; i < count; ++i) {
        const int32_t s = samples[i];
        sum_sq += (uint64_t)(s * s);
    }

    const double mean_sq = (double)sum_sq / (double)count;
    return (float)sqrt(mean_sq);
}
