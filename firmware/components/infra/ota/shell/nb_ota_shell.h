#ifndef NB_OTA_SHELL_H
#define NB_OTA_SHELL_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t nb_ota_shell_confirm_boot_if_pending(void);

esp_err_t nb_ota_shell_rollback_and_reboot(void);

#ifdef __cplusplus
}
#endif

#endif
