#ifndef NB_WATCHDOG_SHELL_H
#define NB_WATCHDOG_SHELL_H

#include <stdint.h>

#include "esp_err.h"
#include "watchdog.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Casca S1.5: inicializa o TWDT do ESP-IDF, registra a task chamadora e
 * persiste em NVS a causa de reset observada no boot anterior.
 */

esp_err_t nb_watchdog_shell_init(uint32_t timeout_ms);
esp_err_t nb_watchdog_shell_feed(void);

nb_watchdog_reset_cause_t nb_watchdog_shell_get_last_reset_cause(void);

#ifdef __cplusplus
}
#endif

#endif
