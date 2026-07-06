#include "nb_energy.h"

#include <string.h>

static float clampf(float v, float lo, float hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

void nb_energy_init(nb_energy_t *e)
{
    if (e == NULL) {
        return;
    }
    memset(e, 0, sizeof(*e));
}

void nb_energy_tick(nb_energy_t *e, uint32_t dt_ms, uint32_t boredom_ms, float arousal,
                    bool quiet_mode)
{
    if (e == NULL) {
        return;
    }

    const float boredom_target =
        clampf((float)boredom_ms / (float)NB_ENERGY_BOREDOM_RAMP_MS, 0.0f, 1.0f);
    const float arousal_pull = clampf(arousal, 0.0f, 1.0f) * NB_ENERGY_AROUSAL_PULL;
    const float quiet_bias = quiet_mode ? NB_ENERGY_QUIET_BIAS : 0.0f;
    const float target = clampf(boredom_target - arousal_pull + quiet_bias, 0.0f, 1.0f);

    const float alpha = clampf((float)dt_ms / NB_ENERGY_LEVEL_TAU_MS, 0.0f, 1.0f);
    e->level += (target - e->level) * alpha;
}
