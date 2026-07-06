#include "nb_breath.h"

#include <math.h>

/* Evita M_PI (não garantida em -std=c17 estrito) -- mesma prática de
 * idle_engine.c (NB_IDLE_PI). */
#define NB_BREATH_PI 3.14159265358979323846f

float nb_breath_scale(uint32_t now_ms, uint32_t period_ms, float amp)
{
    if (period_ms == 0u) {
        return 1.0f;
    }
    const float phase = 2.0f * NB_BREATH_PI * (float)(now_ms % period_ms) / (float)period_ms;
    return 1.0f + amp * sinf(phase);
}
