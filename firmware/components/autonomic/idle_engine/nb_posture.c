#include "nb_posture.h"

#include <string.h>

static uint32_t xorshift32(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/* [0, 1) com 24 bits de precisão -- mesma técnica de nb_attention.c. */
static float rand01(uint32_t *state)
{
    return (float)(xorshift32(state) >> 8) / (float)(1u << 24);
}

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

/* Curva S (3x²-2x³) -- ease-out da transição (velocidade zero na
 * chegada), mesma técnica de nb_attention.c. */
static float smoothstep(float x)
{
    x = clampf(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

static uint32_t sample_hold_ms(nb_posture_t *p)
{
    const uint32_t span = NB_POSTURE_HOLD_MAX_MS - NB_POSTURE_HOLD_MIN_MS;
    return NB_POSTURE_HOLD_MIN_MS + (uint32_t)(rand01(&p->rng_state) * (float)span);
}

static void sample_target(nb_posture_t *p)
{
    p->target_roll = (rand01(&p->rng_state) * 2.0f - 1.0f) * NB_POSTURE_ROLL_AMPLITUDE;
    p->target_gaze_x = (rand01(&p->rng_state) * 2.0f - 1.0f) * NB_POSTURE_GAZE_OFFSET_AMPLITUDE;
    p->target_gaze_y = (rand01(&p->rng_state) * 2.0f - 1.0f) * NB_POSTURE_GAZE_OFFSET_AMPLITUDE;
    p->target_asymmetry = (rand01(&p->rng_state) * 2.0f - 1.0f) * NB_POSTURE_ASYMMETRY_AMPLITUDE;
}

void nb_posture_init(nb_posture_t *p, uint32_t rng_seed)
{
    if (p == NULL) {
        return;
    }
    memset(p, 0, sizeof(*p));
    p->rng_state = (rng_seed != 0u) ? rng_seed : 0x9E3779B9u;
    p->phase = NB_POSTURE_HOLD;
    p->phase_duration_ms = sample_hold_ms(p);
}

void nb_posture_reset_to_center(nb_posture_t *p)
{
    if (p == NULL) {
        return;
    }
    p->phase = NB_POSTURE_HOLD;
    p->phase_started_ms = p->now_ms;
    p->phase_duration_ms = sample_hold_ms(p);
    p->from_roll = 0.0f;
    p->from_gaze_x = 0.0f;
    p->from_gaze_y = 0.0f;
    p->from_asymmetry = 0.0f;
    p->target_roll = 0.0f;
    p->target_gaze_x = 0.0f;
    p->target_gaze_y = 0.0f;
    p->target_asymmetry = 0.0f;
    p->roll = 0.0f;
    p->gaze_x = 0.0f;
    p->gaze_y = 0.0f;
    p->asymmetry = 0.0f;
}

void nb_posture_tick(nb_posture_t *p, uint32_t dt_ms)
{
    if (p == NULL) {
        return;
    }
    p->now_ms += dt_ms;

    const uint32_t elapsed = p->now_ms - p->phase_started_ms;
    if (elapsed >= p->phase_duration_ms) {
        if (p->phase == NB_POSTURE_HOLD) {
            p->from_roll = p->roll;
            p->from_gaze_x = p->gaze_x;
            p->from_gaze_y = p->gaze_y;
            p->from_asymmetry = p->asymmetry;
            sample_target(p);
            p->phase = NB_POSTURE_TRANSITION;
            p->phase_duration_ms = NB_POSTURE_TRANSITION_MS;
            p->phase_started_ms = p->now_ms;
        } else {
            p->phase = NB_POSTURE_HOLD;
            p->phase_duration_ms = sample_hold_ms(p);
            p->phase_started_ms = p->now_ms;
        }
    }

    if (p->phase == NB_POSTURE_TRANSITION) {
        const float t = clampf((float)(p->now_ms - p->phase_started_ms) /
                                       (float)p->phase_duration_ms,
                               0.0f, 1.0f);
        const float ease = smoothstep(t);
        p->roll = p->from_roll + (p->target_roll - p->from_roll) * ease;
        p->gaze_x = p->from_gaze_x + (p->target_gaze_x - p->from_gaze_x) * ease;
        p->gaze_y = p->from_gaze_y + (p->target_gaze_y - p->from_gaze_y) * ease;
        p->asymmetry = p->from_asymmetry + (p->target_asymmetry - p->from_asymmetry) * ease;
    } else {
        p->roll = p->target_roll;
        p->gaze_x = p->target_gaze_x;
        p->gaze_y = p->target_gaze_y;
        p->asymmetry = p->target_asymmetry;
    }
}
