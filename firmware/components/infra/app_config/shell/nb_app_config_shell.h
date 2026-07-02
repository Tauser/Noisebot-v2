#ifndef NB_APP_CONFIG_SHELL_H
#define NB_APP_CONFIG_SHELL_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Casca do app_config (S1.4): persiste as chaves tipadas em NVS (namespace
 * "nb_cfg"), carregando os valores gravados na inicialização e escrevendo
 * só quando o valor muda. Singleton protegido por mutex FreeRTOS, mesmo
 * padrão do logger shell.
 */

esp_err_t nb_app_config_shell_init(void);

bool nb_app_config_shell_get_u32(nb_config_key_t key, uint32_t *out_value);
bool nb_app_config_shell_set_u32(nb_config_key_t key, uint32_t value);

bool nb_app_config_shell_get_i32(nb_config_key_t key, int32_t *out_value);
bool nb_app_config_shell_set_i32(nb_config_key_t key, int32_t value);

#ifdef __cplusplus
}
#endif

#endif
