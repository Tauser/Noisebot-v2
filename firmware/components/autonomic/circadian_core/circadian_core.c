#include "circadian_core.h"

#include <string.h>

#define NB_CIRCADIAN_SECONDS_PER_DAY 86400u
#define NB_CIRCADIAN_SECONDS_PER_HOUR 3600u

nb_circadian_phase_t nb_circadian_core_phase_for_hour(uint32_t hour_of_day) {
    const uint32_t hour = hour_of_day % 24u;
    if (hour >= NB_CIRCADIAN_NIGHT_START_HOUR || hour < NB_CIRCADIAN_DAWN_START_HOUR) {
        return NB_CIRCADIAN_PHASE_NIGHT;
    }
    if (hour < NB_CIRCADIAN_DAY_START_HOUR) {
        return NB_CIRCADIAN_PHASE_DAWN;
    }
    if (hour < NB_CIRCADIAN_DUSK_START_HOUR) {
        return NB_CIRCADIAN_PHASE_DAY;
    }
    return NB_CIRCADIAN_PHASE_DUSK;
}

/* Progresso linear [0,1] de seconds_of_day dentro da janela
 * [start_hour, end_hour). Fora da janela, clampa em 0 ou 1. */
static float ramp_progress(uint32_t seconds_of_day, uint32_t start_hour, uint32_t end_hour) {
    const uint32_t start_s = start_hour * NB_CIRCADIAN_SECONDS_PER_HOUR;
    const uint32_t end_s = end_hour * NB_CIRCADIAN_SECONDS_PER_HOUR;
    if (seconds_of_day <= start_s) {
        return 0.0f;
    }
    if (seconds_of_day >= end_s) {
        return 1.0f;
    }
    return (float)(seconds_of_day - start_s) / (float)(end_s - start_s);
}

static nb_circadian_output_t resolve_output(uint64_t unix_ms, bool has_time_source) {
    nb_circadian_output_t out;
    memset(&out, 0, sizeof(out));
    out.has_time_source = has_time_source;

    const uint32_t seconds_of_day = (uint32_t)((unix_ms / 1000u) % NB_CIRCADIAN_SECONDS_PER_DAY);
    const uint32_t hour = seconds_of_day / NB_CIRCADIAN_SECONDS_PER_HOUR;
    out.phase = nb_circadian_core_phase_for_hour(hour);

    switch (out.phase) {
    case NB_CIRCADIAN_PHASE_NIGHT:
        out.brightness_scale = NB_CIRCADIAN_NIGHT_BRIGHTNESS;
        out.quiet_mode = true;
        break;
    case NB_CIRCADIAN_PHASE_DAY:
        out.brightness_scale = NB_CIRCADIAN_DAY_BRIGHTNESS;
        out.quiet_mode = false;
        break;
    case NB_CIRCADIAN_PHASE_DAWN: {
        const float progress =
            ramp_progress(seconds_of_day, NB_CIRCADIAN_DAWN_START_HOUR, NB_CIRCADIAN_DAY_START_HOUR);
        out.brightness_scale =
            NB_CIRCADIAN_NIGHT_BRIGHTNESS + (NB_CIRCADIAN_DAY_BRIGHTNESS - NB_CIRCADIAN_NIGHT_BRIGHTNESS) * progress;
        out.quiet_mode = false;
        break;
    }
    case NB_CIRCADIAN_PHASE_DUSK:
    default: {
        const float progress =
            ramp_progress(seconds_of_day, NB_CIRCADIAN_DUSK_START_HOUR, NB_CIRCADIAN_NIGHT_START_HOUR);
        out.brightness_scale =
            NB_CIRCADIAN_DAY_BRIGHTNESS - (NB_CIRCADIAN_DAY_BRIGHTNESS - NB_CIRCADIAN_NIGHT_BRIGHTNESS) * progress;
        out.quiet_mode = false;
        break;
    }
    }

    if (!has_time_source) {
        /* Default neutro: nunca força NIGHT/quiet antes de saber a hora
         * de verdade (âncora real ou fallback NVS). */
        out.phase = NB_CIRCADIAN_PHASE_DAY;
        out.brightness_scale = NB_CIRCADIAN_DAY_BRIGHTNESS;
        out.quiet_mode = false;
    }

    return out;
}

void nb_circadian_core_init(nb_circadian_core_t *core) {
    if (!core) {
        return;
    }
    memset(core, 0, sizeof(*core));
}

void nb_circadian_core_set_time_anchor(nb_circadian_core_t *core, uint64_t unix_ms) {
    if (!core) {
        return;
    }
    core->anchor_unix_ms = unix_ms;
    core->anchor_monotonic_ms = core->monotonic_now_ms;
    core->has_anchor = true;
}

uint64_t nb_circadian_core_now_unix_ms(const nb_circadian_core_t *core) {
    if (!core || !core->has_anchor) {
        return 0u;
    }
    const uint64_t elapsed = core->monotonic_now_ms - core->anchor_monotonic_ms;
    return core->anchor_unix_ms + elapsed;
}

nb_circadian_output_t nb_circadian_core_tick(nb_circadian_core_t *core, uint32_t dt_ms) {
    if (!core) {
        return resolve_output(0u, false);
    }
    core->monotonic_now_ms += dt_ms;
    return resolve_output(nb_circadian_core_now_unix_ms(core), core->has_anchor);
}
