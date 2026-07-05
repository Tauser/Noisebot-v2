#include "led_service.h"

#include <math.h>
#include <string.h>

#define NB_LED_PI 3.14159265358979323846f

/* Leve defasagem entre os 2 LEDs em respiração -- evita ar mecânico
 * (herdado do v1, NB_LED_BREATHE_PHASE_OFFSET_MS). */
#define NB_LED_BREATHE_PHASE_OFFSET_MS 200u

/* Overlay de toque: sobe rápido, decai longo (v1 led_effect_touch: flash
 * quente + fade longo, BEHAVIOR.md §4/VISUAL.md §6). */
#define NB_LED_TOUCH_OVERLAY_FLASH_MS 150u
#define NB_LED_TOUCH_OVERLAY_FADE_MS 900u
#define NB_LED_TOUCH_OVERLAY_TOTAL_MS (NB_LED_TOUCH_OVERLAY_FLASH_MS + NB_LED_TOUCH_OVERLAY_FADE_MS)

typedef enum {
    NB_LED_WAVE_STATIC = 0, /* cor fixa em min_level (SLEEPING, TOUCH_REACTING) */
    NB_LED_WAVE_BREATHE,    /* seno suave entre min_level e 1.0 */
    NB_LED_WAVE_BLINK,      /* quadrada on/off (ERROR) */
} nb_led_wave_t;

typedef struct {
    nb_led_color_t color;
    uint32_t period_ms; /* ignorado se wave == STATIC */
    nb_led_wave_t wave;
    float min_level; /* piso da onda, ou nível fixo se STATIC */
} nb_led_state_entry_t;

/* VISUAL.md §6: IDLE quente/respiração ~6s, ATTENTIVE frio/pulso médio,
 * SLEEPING quase apagado, ERROR vermelho intermitente (P0, nunca
 * suprimido). BOOT/SAFE_MODE sem cor definida em VISUAL.md -- default de
 * engenharia documentado no README (branco frio / laranja, paridade
 * estrutural com o v1). RESPONDING com pulso fixo até S4.3 trazer
 * amplitude real de fala. */
static const nb_led_state_entry_t NB_LED_STATE_TABLE[NB_FSM_STATE_COUNT] = {
    [NB_FSM_STATE_BOOT] = {{200, 220, 255}, 1200u, NB_LED_WAVE_BREATHE, 0.35f},
    [NB_FSM_STATE_IDLE] = {{255, 150, 60}, 6000u, NB_LED_WAVE_BREATHE, 0.25f},
    [NB_FSM_STATE_ATTENTIVE] = {{140, 200, 255}, 1800u, NB_LED_WAVE_BREATHE, 0.45f},
    [NB_FSM_STATE_RESPONDING] = {{100, 255, 220}, 1300u, NB_LED_WAVE_BREATHE, 0.28f},
    [NB_FSM_STATE_TOUCH_REACTING] = {{255, 140, 60}, 0u, NB_LED_WAVE_STATIC, 1.0f},
    [NB_FSM_STATE_SLEEPING] = {{10, 20, 40}, 0u, NB_LED_WAVE_STATIC, 0.02f},
    [NB_FSM_STATE_ERROR] = {{255, 0, 0}, 600u, NB_LED_WAVE_BLINK, 0.12f},
    [NB_FSM_STATE_SAFE_MODE] = {{255, 80, 0}, 2500u, NB_LED_WAVE_BREATHE, 0.35f},
};

static const nb_led_color_t NB_LED_TOUCH_OVERLAY_COLOR = {255, 190, 120};

static float clampf(float v, float lo, float hi) {
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static float wave_level(const nb_led_state_entry_t *entry, uint32_t phase_ms) {
    if (entry->wave == NB_LED_WAVE_STATIC || entry->period_ms == 0u) {
        return entry->min_level;
    }

    const uint32_t t = phase_ms % entry->period_ms;
    if (entry->wave == NB_LED_WAVE_BLINK) {
        return (t < entry->period_ms / 2u) ? 1.0f : entry->min_level;
    }

    const float angle = 2.0f * NB_LED_PI * (float)t / (float)entry->period_ms;
    const float raw = 0.5f + 0.5f * sinf(angle);
    return entry->min_level + (1.0f - entry->min_level) * raw;
}

/* Sobe linear 0->1 até FLASH_MS, depois decai linear 1->0 até TOTAL_MS. */
static float touch_overlay_level(uint32_t elapsed_ms) {
    if (elapsed_ms < NB_LED_TOUCH_OVERLAY_FLASH_MS) {
        return (float)elapsed_ms / (float)NB_LED_TOUCH_OVERLAY_FLASH_MS;
    }
    if (elapsed_ms >= NB_LED_TOUCH_OVERLAY_TOTAL_MS) {
        return 0.0f;
    }
    const uint32_t fade_elapsed = elapsed_ms - NB_LED_TOUCH_OVERLAY_FLASH_MS;
    return 1.0f - (float)fade_elapsed / (float)NB_LED_TOUCH_OVERLAY_FADE_MS;
}

static nb_led_color_t blend_color(nb_led_color_t base, nb_led_color_t overlay, float t) {
    nb_led_color_t out;
    out.r = (uint8_t)((float)base.r + ((float)overlay.r - (float)base.r) * t);
    out.g = (uint8_t)((float)base.g + ((float)overlay.g - (float)base.g) * t);
    out.b = (uint8_t)((float)base.b + ((float)overlay.b - (float)base.b) * t);
    return out;
}

/* Gamma 2.2 (percepção humana de brilho, mesma curva do v1). */
static uint8_t gamma_correct(float level_0_to_1) {
    const float clamped = clampf(level_0_to_1, 0.0f, 1.0f);
    const float corrected = powf(clamped, 2.2f);
    return (uint8_t)(corrected * 255.0f + 0.5f);
}

static nb_led_color_t resolve_pixel(nb_led_color_t base, float level, float brightness_scale) {
    nb_led_color_t out;
    const float scale = level * brightness_scale;
    out.r = gamma_correct(((float)base.r / 255.0f) * scale);
    out.g = gamma_correct(((float)base.g / 255.0f) * scale);
    out.b = gamma_correct(((float)base.b / 255.0f) * scale);
    return out;
}

void nb_led_service_init(nb_led_service_t *svc) {
    if (!svc) {
        return;
    }
    memset(svc, 0, sizeof(*svc));
    svc->state = NB_FSM_STATE_BOOT;
    svc->brightness_scale = 1.0f;
}

void nb_led_service_set_state(nb_led_service_t *svc, nb_fsm_state_t state) {
    if (!svc || state >= NB_FSM_STATE_COUNT) {
        return;
    }
    if (svc->state != state) {
        svc->state = state;
        svc->state_elapsed_ms = 0u;
    }
}

void nb_led_service_trigger_touch(nb_led_service_t *svc) {
    if (!svc) {
        return;
    }
    if (svc->state == NB_FSM_STATE_ERROR || svc->state == NB_FSM_STATE_SAFE_MODE) {
        return; /* overlays nunca suprimem P0 (BEHAVIOR.md §3, paridade v1) */
    }
    svc->touch_overlay_active = true;
    svc->touch_overlay_elapsed_ms = 0u;
}

void nb_led_service_set_brightness_scale(nb_led_service_t *svc, float scale) {
    if (!svc) {
        return;
    }
    svc->brightness_scale = clampf(scale, 0.0f, 1.0f);
}

bool nb_led_service_tick(nb_led_service_t *svc, uint32_t dt_ms, nb_led_frame_t *out_frame) {
    if (!svc || !out_frame) {
        return false;
    }

    svc->state_elapsed_ms += dt_ms;
    if (svc->touch_overlay_active) {
        svc->touch_overlay_elapsed_ms += dt_ms;
        if (svc->touch_overlay_elapsed_ms >= NB_LED_TOUCH_OVERLAY_TOTAL_MS) {
            svc->touch_overlay_active = false;
        }
    }

    const nb_led_state_entry_t *entry = &NB_LED_STATE_TABLE[svc->state];
    const float overlay_level =
        svc->touch_overlay_active ? touch_overlay_level(svc->touch_overlay_elapsed_ms) : 0.0f;

    nb_led_frame_t frame;
    for (uint32_t i = 0; i < NB_LED_SERVICE_COUNT; ++i) {
        uint32_t phase_ms = svc->state_elapsed_ms;
        if (entry->wave == NB_LED_WAVE_BREATHE && i == 1u) {
            phase_ms += NB_LED_BREATHE_PHASE_OFFSET_MS;
        }

        float level = wave_level(entry, phase_ms);
        nb_led_color_t color = entry->color;

        if (overlay_level > 0.0f) {
            color = blend_color(color, NB_LED_TOUCH_OVERLAY_COLOR, overlay_level);
            level = level + (1.0f - level) * overlay_level;
        }

        frame.pixels[i] = resolve_pixel(color, level, svc->brightness_scale);
    }

    const bool changed = !svc->has_last_frame || memcmp(&frame, &svc->last_frame, sizeof(frame)) != 0;
    svc->last_frame = frame;
    svc->has_last_frame = true;
    *out_frame = frame;
    return changed;
}
