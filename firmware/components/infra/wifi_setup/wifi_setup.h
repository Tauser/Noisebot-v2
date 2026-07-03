#ifndef NB_WIFI_SETUP_H
#define NB_WIFI_SETUP_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro do fluxo de provisioning WiFi (S1.6).
 *
 * Sem FreeRTOS, sem esp_*, sem NVS, sem rede: só validação de SSID/senha e a
 * máquina de estados do fluxo de provisioning. A casca (wifi_prov_mgr do
 * ESP-IDF sobre SoftAP) fica em shell/ — ver README.md deste componente.
 */

#define NB_WIFI_SETUP_SSID_MAX_LEN 32u
#define NB_WIFI_SETUP_PASSWORD_MIN_LEN 8u
#define NB_WIFI_SETUP_PASSWORD_MAX_LEN 63u

typedef enum {
    NB_WIFI_SETUP_STATE_NOT_PROVISIONED = 0,
    NB_WIFI_SETUP_STATE_PROVISIONING = 1,
    NB_WIFI_SETUP_STATE_PROVISIONED = 2,
    NB_WIFI_SETUP_STATE_FAILED = 3,
} nb_wifi_setup_state_t;

typedef struct {
    nb_wifi_setup_state_t state;
} nb_wifi_setup_t;

void nb_wifi_setup_init(nb_wifi_setup_t *setup);

nb_wifi_setup_state_t nb_wifi_setup_get_state(const nb_wifi_setup_t *setup);

/* 1..NB_WIFI_SETUP_SSID_MAX_LEN bytes, sem '\0' embutido antes do fim. */
bool nb_wifi_setup_validate_ssid(const char *ssid);

/* Vazia (rede aberta) ou 8..63 chars ASCII imprimíveis (0x20..0x7e) para
 * WPA2-PSK. */
bool nb_wifi_setup_validate_password(const char *password);

/* NOT_PROVISIONED ou FAILED -> PROVISIONING. Retorna false (sem mudar
 * estado) se já houver provisioning em andamento ou já provisionado. */
bool nb_wifi_setup_begin(nb_wifi_setup_t *setup);

/* Só válido em PROVISIONING. Valida ssid/password; se válidos, vai para
 * PROVISIONED e retorna true. Se inválidos, vai para FAILED e retorna
 * false. Fora de PROVISIONING, retorna false sem mudar estado. */
bool nb_wifi_setup_complete(nb_wifi_setup_t *setup, const char *ssid,
                           const char *password);

/* Só válido em PROVISIONING (ex.: AP rejeitou a senha). Vai para FAILED.
 * Retorna false sem mudar estado se não estiver em PROVISIONING. */
bool nb_wifi_setup_fail(nb_wifi_setup_t *setup);

/* Qualquer estado -> NOT_PROVISIONED (reset explícito do provisioning). */
void nb_wifi_setup_reset(nb_wifi_setup_t *setup);

#ifdef __cplusplus
}
#endif

#endif
