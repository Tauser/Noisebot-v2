#ifndef NB_LED_SERVICE_H
#define NB_LED_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "tiny_fsm.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro do led_service (S3.3, camada L3, VISUAL.md §6). Reaproveita
 * nb_fsm_state_t do tiny_fsm (sem duplicar enum, mesmo padrão de
 * emotion_core<->renderer, ARCHITECTURE.md §2) pra escolher cor/período de
 * respiração por estado. Modelo two-layer herdado do v1: base (por estado)
 * + overlay de toque (one-shot, flash quente + fade longo), com
 * ERROR/SAFE_MODE nunca suprimidos por overlay.
 *
 * "Brilho circadiano" é só o mecanismo aqui (multiplicador injetado em
 * [0,1]) -- a fonte real de hora-do-dia chega em S3.4. Gamma 2.2 aplicado
 * na saída (percepção humana de brilho, mesma curva do v1).
 *
 * Sem FreeRTOS/ESP-IDF: dt_ms injetado via nb_led_service_tick(). Sem
 * timestamp absoluto -- só contadores de tempo decorrido, resetados por
 * nb_led_service_set_state()/nb_led_service_trigger_touch(), pra não
 * precisar de uma base de clock compartilhada com quem chama.
 */

#define NB_LED_SERVICE_COUNT 2u /* NB_HW_LED_COUNT (nb_hw_config.h) -- núcleo puro não depende de HAL */

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} nb_led_color_t;

typedef struct {
    nb_led_color_t pixels[NB_LED_SERVICE_COUNT];
} nb_led_frame_t;

typedef struct {
    nb_fsm_state_t state;
    uint32_t state_elapsed_ms;

    bool touch_overlay_active;
    uint32_t touch_overlay_elapsed_ms;

    float brightness_scale; /* [0,1], circadiano -- multiplica antes do gamma */

    nb_led_frame_t last_frame;
    bool has_last_frame;
} nb_led_service_t;

/* Estado inicial: BOOT, brightness_scale=1.0, sem overlay. */
void nb_led_service_init(nb_led_service_t *svc);

/* Muda o estado-base (do tiny_fsm) -- reseta a fase de respiração quando o
 * estado muda de fato; repetir o mesmo estado é no-op na fase. */
void nb_led_service_set_state(nb_led_service_t *svc, nb_fsm_state_t state);

/* Dispara o overlay de toque (flash quente + fade longo, ~1050ms total).
 * Ignorado (no-op) se o estado-base atual for ERROR ou SAFE_MODE --
 * overlays nunca suprimem P0 (BEHAVIOR.md §3, paridade v1). */
void nb_led_service_trigger_touch(nb_led_service_t *svc);

/* Multiplicador de brilho circadiano, clampado em [0,1]. */
void nb_led_service_set_brightness_scale(nb_led_service_t *svc, float scale);

/* Avança dt_ms, resolve a cor de cada LED (base + overlay + brilho +
 * gamma) em *out_frame. Retorna true se o frame mudou desde a última
 * chamada -- a casca só escreve no RMT quando true (mecanismo direto
 * contra flicker/tráfego redundante, herdado do v1). */
bool nb_led_service_tick(nb_led_service_t *svc, uint32_t dt_ms, nb_led_frame_t *out_frame);

#ifdef __cplusplus
}
#endif

#endif
