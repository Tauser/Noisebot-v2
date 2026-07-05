#ifndef NB_LED_SERVICE_SHELL_H
#define NB_LED_SERVICE_SHELL_H

#include "esp_err.h"
#include "led_service.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Casca do led_service (S3.3): dona da instância única do núcleo puro +
 * led_hal (RMT). Chamada de dentro da task de face (mesmo tick de face,
 * main.c) -- não roda em task própria.
 */

esp_err_t nb_led_service_shell_init(void);

/* Tick por frame com o estado atual do tiny_fsm; escreve no RMT só quando
 * o frame muda (dirty-flag do núcleo, evita tráfego/flicker redundante). */
void nb_led_service_shell_tick(nb_fsm_state_t state, uint32_t dt_ms);

/* Chamado por reflex_engine_shell quando aplica um evento de toque
 * (mesmo lugar que já aplica em emotion_core/tiny_fsm). */
void nb_led_service_shell_trigger_touch(void);

/* [0,1], circadiano -- mecanismo de S3.3, fonte real de hora-do-dia em S3.4. */
void nb_led_service_shell_set_brightness_scale(float scale);

#ifdef __cplusplus
}
#endif

#endif
