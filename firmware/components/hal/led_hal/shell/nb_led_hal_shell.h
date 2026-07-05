#ifndef NB_LED_HAL_SHELL_H
#define NB_LED_HAL_SHELL_H

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Casca fina sobre espressif/led_strip (RMT), GPIO21, NB_HW_LED_COUNT
 * WS2812 em cadeia (S3.3, HARDWARE.md). Sem lógica não-trivial pra
 * extrair -- só inicializa o driver e escreve pixels; a lógica de
 * estado/cor/prioridade/gamma mora no núcleo puro de led_service.
 */

esp_err_t nb_led_hal_shell_init(void);

/* index < NB_HW_LED_COUNT. RGB já gamma-corrigido pelo chamador. */
esp_err_t nb_led_hal_shell_set_pixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b);

/* Envia o buffer acumulado pro strip via RMT. */
esp_err_t nb_led_hal_shell_refresh(void);

#ifdef __cplusplus
}
#endif

#endif
