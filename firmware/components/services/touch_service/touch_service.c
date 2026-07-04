#include "touch_service.h"

#include <stddef.h>

/* Constantes herdadas do touch_service validado em produto no v1 (ver
 * plano do S3.1 em docs/ROADMAP.md para a referência completa). */
#define NB_TOUCH_LONG_PRESS_MS 1500u
#define NB_TOUCH_SUSTAINED_MS 3000u

#define NB_TOUCH_SENS_DEFAULT 0.20f
/* Piso baixo (0.2% em vez de um piso típico de 1-5%): o pad de cobre do
 * hardware validado no v1 gera variação de ~1% no toque, bem abaixo do
 * range comercial típico (20-50%). Com um piso maior o threshold ficaria
 * alto demais pra esse hardware específico. */
#define NB_TOUCH_SENS_MIN 0.002f
#define NB_TOUCH_SENS_MAX 1.00f
#define NB_TOUCH_HYST_FACTOR 0.40f /* threshold_off = span * HYST_FACTOR */

/* 3 amostras consecutivas (~60ms a 50Hz) pra confirmar toque; mesmo tanto
 * pra confirmar release. */
#define NB_TOUCH_DEBOUNCE_ON_COUNT 3u
#define NB_TOUCH_DEBOUNCE_OFF_COUNT 3u
/* Após o debounce confirmar entrada em TOUCHING, só dispara TAP/WAKE se o
 * sinal segurar por mais este tempo -- rejeita proximidade (campo
 * capacitivo do fio/fita) sem atrasar LONG_PRESS/SUSTAINED. */
#define NB_TOUCH_TAP_HOLD_MIN_MS 100u

#define NB_TOUCH_SIGNAL_EMA_ALPHA 0.20f /* suavização do sinal, τ≈4 ticks */

#define NB_TOUCH_BOOT_STABLE_MS 1000u  /* nenhum evento dispara antes disso */
#define NB_TOUCH_RECALIB_BOOT_MS 2000u /* recalibração lenta só começa depois */
#define NB_TOUCH_RECALIB_ALPHA 0.001f  /* EMA do baseline, τ≈20s */
#define NB_TOUCH_NOISE_SPREAD_PCT 2u   /* spread máx aceito pra recalibrar (%) */
/* Se preso em SUSTAINED por mais que isso com sinal estável, assume drift
 * de baseline (não toque real) e força re-baseline. */
#define NB_TOUCH_RECALIB_STUCK_MS 10000u

static void recompute_thresholds(nb_touch_service_t *svc)
{
    const float span = (float)svc->baseline * svc->sensitivity;
    svc->threshold_on = svc->baseline + (uint32_t)span;
    svc->threshold_off = svc->baseline + (uint32_t)(span * NB_TOUCH_HYST_FACTOR);
}

static void noise_push(nb_touch_service_t *svc, uint32_t raw)
{
    svc->noise_history[svc->noise_idx] = raw;
    svc->noise_idx = (uint8_t)((svc->noise_idx + 1u) % NB_TOUCH_SERVICE_NOISE_WINDOW);
    if (svc->noise_idx == 0u) {
        svc->noise_history_full = true;
    }
}

/* Spread das últimas NB_TOUCH_SERVICE_NOISE_WINDOW amostras < NB_TOUCH_
 * NOISE_SPREAD_PCT% do baseline -- usado pra saber se é seguro recalibrar
 * (sinal parado) ou se a queda em SUSTAINED_ACTIVE é drift, não toque. */
static bool is_signal_stable(const nb_touch_service_t *svc)
{
    if (!svc->noise_history_full) {
        return false;
    }

    uint32_t hi = 0;
    uint32_t lo = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < NB_TOUCH_SERVICE_NOISE_WINDOW; ++i) {
        if (svc->noise_history[i] > hi) {
            hi = svc->noise_history[i];
        }
        if (svc->noise_history[i] < lo) {
            lo = svc->noise_history[i];
        }
    }
    const uint32_t spread = hi - lo;
    const uint32_t limit = (svc->baseline * NB_TOUCH_NOISE_SPREAD_PCT) / 100u;
    return spread <= limit;
}

void nb_touch_service_init(nb_touch_service_t *svc, uint32_t baseline)
{
    if (svc == NULL) {
        return;
    }

    svc->state = NB_TOUCH_STATE_IDLE;
    svc->press_ms = 0;
    svc->sensitivity = NB_TOUCH_SENS_DEFAULT;
    svc->baseline = baseline;
    svc->debounce_on = 0;
    svc->debounce_off = 0;
    svc->filtered_raw = 0.0f;
    svc->uptime_ms = 0;
    svc->boot_stable = false;
    svc->noise_idx = 0;
    svc->noise_history_full = false;
    svc->sleeping = false;
    svc->last_raw = 0;
    svc->stuck_ms = 0;

    recompute_thresholds(svc);
}

void nb_touch_service_set_sensitivity(nb_touch_service_t *svc, float factor)
{
    if (svc == NULL) {
        return;
    }
    if (factor < NB_TOUCH_SENS_MIN) {
        factor = NB_TOUCH_SENS_MIN;
    }
    if (factor > NB_TOUCH_SENS_MAX) {
        factor = NB_TOUCH_SENS_MAX;
    }
    svc->sensitivity = factor;
    recompute_thresholds(svc);
}

void nb_touch_service_set_sleeping(nb_touch_service_t *svc, bool sleeping)
{
    if (svc == NULL) {
        return;
    }
    svc->sleeping = sleeping;
}

bool nb_touch_service_tick(nb_touch_service_t *svc, uint32_t raw, uint32_t dt_ms,
                           nb_touch_event_t *out_event)
{
    if (svc == NULL) {
        return false;
    }

    svc->uptime_ms += dt_ms;
    svc->last_raw = raw;

    /* Boot stabilization: coleta amostras, não dispara nada. */
    if (!svc->boot_stable) {
        noise_push(svc, raw);
        if (svc->uptime_ms >= NB_TOUCH_BOOT_STABLE_MS) {
            svc->boot_stable = true;
        }
        return false;
    }

    noise_push(svc, raw);

    /* EMA de sinal -- a FSM opera sobre isso, não sobre raw bruto. */
    if (svc->filtered_raw == 0.0f) {
        svc->filtered_raw = (float)raw;
    } else {
        svc->filtered_raw =
            svc->filtered_raw * (1.0f - NB_TOUCH_SIGNAL_EMA_ALPHA) + (float)raw * NB_TOUCH_SIGNAL_EMA_ALPHA;
    }
    const uint32_t fraw = (uint32_t)svc->filtered_raw;

    /* Recalibração lenta: só em IDLE, sinal abaixo do threshold e estável
     * -- proteção contra "baseline poisoning" durante um toque real. */
    if (svc->uptime_ms >= NB_TOUCH_RECALIB_BOOT_MS && svc->state == NB_TOUCH_STATE_IDLE &&
        fraw < svc->threshold_off && is_signal_stable(svc)) {
        const float new_baseline =
            (float)svc->baseline * (1.0f - NB_TOUCH_RECALIB_ALPHA) + svc->filtered_raw * NB_TOUCH_RECALIB_ALPHA;
        const uint32_t nb = (uint32_t)new_baseline;
        if (nb != svc->baseline) {
            svc->baseline = nb;
            recompute_thresholds(svc);
        }
    }

    const bool above_on = fraw >= svc->threshold_on;
    const bool below_off = fraw < svc->threshold_off;

    if (above_on) {
        if (svc->debounce_on < NB_TOUCH_DEBOUNCE_ON_COUNT) {
            svc->debounce_on++;
        }
    } else {
        svc->debounce_on = 0;
    }
    if (below_off) {
        if (svc->debounce_off < NB_TOUCH_DEBOUNCE_OFF_COUNT) {
            svc->debounce_off++;
        }
    } else {
        svc->debounce_off = 0;
    }

    const bool confirmed_touch = svc->debounce_on >= NB_TOUCH_DEBOUNCE_ON_COUNT;
    const bool confirmed_release = svc->debounce_off >= NB_TOUCH_DEBOUNCE_OFF_COUNT;

    switch (svc->state) {
    case NB_TOUCH_STATE_IDLE:
        if (confirmed_touch) {
            svc->state = NB_TOUCH_STATE_TOUCHING;
            svc->press_ms = 0;
            svc->debounce_off = 0;
            svc->debounce_on = 0;
            /* TAP/WAKE não dispara aqui -- espera NB_TOUCH_TAP_HOLD_MIN_MS
             * em TOUCHING pra rejeitar proximidade. */
        }
        break;

    case NB_TOUCH_STATE_TOUCHING:
        if (confirmed_release) {
            svc->state = NB_TOUCH_STATE_IDLE;
            svc->press_ms = 0;
        } else {
            const uint32_t prev_ms = svc->press_ms;
            svc->press_ms += dt_ms;
            if (prev_ms < NB_TOUCH_TAP_HOLD_MIN_MS && svc->press_ms >= NB_TOUCH_TAP_HOLD_MIN_MS) {
                if (out_event != NULL) {
                    *out_event = svc->sleeping ? NB_TOUCH_EVENT_WAKE : NB_TOUCH_EVENT_TAP;
                }
                return true;
            }
            if (svc->press_ms >= NB_TOUCH_LONG_PRESS_MS) {
                svc->state = NB_TOUCH_STATE_LONG_PRESSING;
                if (out_event != NULL) {
                    *out_event = NB_TOUCH_EVENT_LONG_PRESS;
                }
                return true;
            }
        }
        break;

    case NB_TOUCH_STATE_LONG_PRESSING:
        if (confirmed_release) {
            svc->state = NB_TOUCH_STATE_IDLE;
            svc->press_ms = 0;
        } else {
            svc->press_ms += dt_ms;
            if (svc->press_ms >= NB_TOUCH_SUSTAINED_MS) {
                svc->state = NB_TOUCH_STATE_SUSTAINED_ACTIVE;
                if (out_event != NULL) {
                    *out_event = NB_TOUCH_EVENT_SUSTAINED;
                }
                return true;
            }
        }
        break;

    case NB_TOUCH_STATE_SUSTAINED_ACTIVE:
        if (confirmed_release) {
            svc->state = NB_TOUCH_STATE_IDLE;
            svc->press_ms = 0;
            svc->stuck_ms = 0;
        } else {
            svc->press_ms += dt_ms;
            svc->stuck_ms += dt_ms;
            /* Preso em SUSTAINED por muito tempo com sinal estável: drift
             * de baseline, não toque real -- força re-baseline. */
            if (svc->stuck_ms >= NB_TOUCH_RECALIB_STUCK_MS && is_signal_stable(svc)) {
                svc->baseline = (uint32_t)svc->filtered_raw;
                recompute_thresholds(svc);
                svc->state = NB_TOUCH_STATE_IDLE;
                svc->press_ms = 0;
                svc->stuck_ms = 0;
                svc->debounce_on = 0;
                svc->debounce_off = 0;
            }
        }
        break;

    default:
        break;
    }

    return false;
}

bool nb_touch_service_is_pressed(const nb_touch_service_t *svc)
{
    return (svc != NULL) && (svc->state != NB_TOUCH_STATE_IDLE);
}

uint32_t nb_touch_service_get_duration_ms(const nb_touch_service_t *svc)
{
    return (svc != NULL) ? svc->press_ms : 0u;
}
