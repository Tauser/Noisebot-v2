#ifndef NB_TOUCH_HAL_SHELL_H
#define NB_TOUCH_HAL_SHELL_H

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Casca do touch_hal (S3.1): periférico touch nativo do ESP-IDF
 * (`driver/touch_sens.h`), GPIO2 / canal 2 (`HARDWARE.md`: "TOUCH2").
 */

/* Configura o canal, faz o settle (200ms sem tocar) e calibra o baseline
 * (10 amostras em 100ms, ver `touch_hal.h`). Demora ~300ms. */
esp_err_t nb_touch_hal_shell_init(uint32_t *out_baseline);

/* Lê o dado suavizado pelo filtro de hardware do canal. */
esp_err_t nb_touch_hal_shell_read(uint32_t *raw);

void nb_touch_hal_shell_deinit(void);

#ifdef __cplusplus
}
#endif

#endif
