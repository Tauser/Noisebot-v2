#include "nb_attention.h"

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

/* [0, 1) com 24 bits de precisão -- mesma técnica de idle_engine.c. */
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

/* Curva S (3x²-2x³) -- ease-out da sacada (velocidade zero na chegada). */
static float smoothstep(float x)
{
    x = clampf(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

static uint32_t sample_fixation_ms(nb_attention_t *att)
{
    const uint32_t span = NB_ATTENTION_FIXATION_MAX_MS - NB_ATTENTION_FIXATION_MIN_MS;
    return NB_ATTENTION_FIXATION_MIN_MS + (uint32_t)(rand01(&att->rng_state) * (float)span);
}

/* ~40% dos alvos voltam para perto do centro (viés "olhar pro usuário");
 * os demais são uniformes no envelope inteiro. */
static void sample_target(nb_attention_t *att, float *tx, float *ty)
{
    if (rand01(&att->rng_state) < NB_ATTENTION_CENTER_BIAS_PROB) {
        *tx = (rand01(&att->rng_state) * 2.0f - 1.0f) * NB_ATTENTION_CENTER_BIAS_RADIUS;
        *ty = (rand01(&att->rng_state) * 2.0f - 1.0f) * NB_ATTENTION_CENTER_BIAS_RADIUS;
    } else {
        *tx = (rand01(&att->rng_state) * 2.0f - 1.0f) * NB_ATTENTION_ENVELOPE;
        *ty = (rand01(&att->rng_state) * 2.0f - 1.0f) * NB_ATTENTION_ENVELOPE;
    }
}

void nb_attention_init(nb_attention_t *att, uint32_t rng_seed)
{
    if (att == NULL) {
        return;
    }
    memset(att, 0, sizeof(*att));
    att->rng_state = (rng_seed != 0u) ? rng_seed : 0x9E3779B9u;
    att->phase = NB_ATTENTION_FIXATE;
    att->phase_duration_ms = sample_fixation_ms(att);
}

void nb_attention_set_saccade_callback(nb_attention_t *att, nb_attention_saccade_cb_t cb,
                                       void *ctx)
{
    if (att == NULL) {
        return;
    }
    att->on_saccade = cb;
    att->on_saccade_ctx = ctx;
}

void nb_attention_tick(nb_attention_t *att, uint32_t dt_ms, float *gaze_x, float *gaze_y)
{
    if (att == NULL) {
        return;
    }
    att->now_ms += dt_ms;

    const uint32_t elapsed = att->now_ms - att->phase_started_ms;
    if (elapsed >= att->phase_duration_ms) {
        if (att->phase == NB_ATTENTION_FIXATE) {
            att->from_x = att->target_x;
            att->from_y = att->target_y;
            sample_target(att, &att->target_x, &att->target_y);
            att->phase = NB_ATTENTION_SACCADE;
            att->phase_duration_ms = NB_ATTENTION_SACCADE_MS;
            att->phase_started_ms = att->now_ms;
            if (att->on_saccade != NULL) {
                att->on_saccade(att->on_saccade_ctx);
            }
        } else {
            att->phase = NB_ATTENTION_FIXATE;
            att->phase_duration_ms = sample_fixation_ms(att);
            att->phase_started_ms = att->now_ms;
        }
    }

    float gx;
    float gy;
    if (att->phase == NB_ATTENTION_SACCADE) {
        const float t = clampf((float)(att->now_ms - att->phase_started_ms) /
                                       (float)att->phase_duration_ms,
                               0.0f, 1.0f);
        const float ease = smoothstep(t);
        gx = att->from_x + (att->target_x - att->from_x) * ease;
        gy = att->from_y + (att->target_y - att->from_y) * ease;
    } else {
        /* Micro-tremor suavizado: alvo re-sorteado a cada 150-350ms,
         * aproximado por suavização exponencial (mesma técnica do
         * SOFT_DRIFT em idle_engine.c) -- ruído branco puro por tick lia
         * como flicker de alta frequência em bancada, não vida sutil. */
        if (att->now_ms >= att->tremor_next_resample_ms) {
            att->tremor_target_x =
                (rand01(&att->rng_state) * 2.0f - 1.0f) * NB_ATTENTION_TREMOR_AMPLITUDE;
            att->tremor_target_y =
                (rand01(&att->rng_state) * 2.0f - 1.0f) * NB_ATTENTION_TREMOR_AMPLITUDE;
            const uint32_t span =
                NB_ATTENTION_TREMOR_RESAMPLE_MAX_MS - NB_ATTENTION_TREMOR_RESAMPLE_MIN_MS;
            att->tremor_next_resample_ms = att->now_ms + NB_ATTENTION_TREMOR_RESAMPLE_MIN_MS +
                                           (uint32_t)(rand01(&att->rng_state) * (float)span);
        }
        const float tremor_alpha = clampf((float)dt_ms / NB_ATTENTION_TREMOR_TAU_MS, 0.0f, 1.0f);
        att->tremor_x += (att->tremor_target_x - att->tremor_x) * tremor_alpha;
        att->tremor_y += (att->tremor_target_y - att->tremor_y) * tremor_alpha;

        gx = att->target_x + att->tremor_x;
        gy = att->target_y + att->tremor_y;
    }

    gx = clampf(gx, -NB_ATTENTION_ENVELOPE, NB_ATTENTION_ENVELOPE);
    gy = clampf(gy, -NB_ATTENTION_ENVELOPE, NB_ATTENTION_ENVELOPE);

    if (gaze_x != NULL) {
        *gaze_x = gx;
    }
    if (gaze_y != NULL) {
        *gaze_y = gy;
    }
}
