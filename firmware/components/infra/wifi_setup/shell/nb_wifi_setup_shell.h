#ifndef NB_WIFI_SETUP_SHELL_H
#define NB_WIFI_SETUP_SHELL_H

#include "esp_err.h"
#include "wifi_setup.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Casca do wifi_setup (S1.6): wifi_prov_mgr do ESP-IDF sobre SoftAP.
 *
 * Se o dispositivo já tem credenciais WiFi persistidas (NVS, via
 * wifi_prov_mgr_is_provisioned()), conecta direto em modo estação. Caso
 * contrário, sobe um SoftAP "NoiseBot2-XXXX" (XXXX = sufixo do MAC) com
 * WIFI_PROV_SECURITY_1 (PoP fixo, logado no console — sem TLS, compatível
 * com a regra de rede LAN local) para o app oficial "ESP SoftAP
 * Provisioning" da Espressif provisionar SSID/senha.
 *
 * O token NBP/2 não é tratado aqui — ver ajuste de escopo em
 * docs/ROADMAP.md §S1.6 e README.md deste componente.
 */
esp_err_t nb_wifi_setup_shell_init(void);

nb_wifi_setup_state_t nb_wifi_setup_shell_get_state(void);

/* S3.8, item 8 (STATUS): RSSI do AP atual via esp_wifi_sta_get_ap_info().
 * -128 (sentinela, fora da faixa real de RSSI de WiFi -- tipicamente
 * -30 a -90) se não conectado em modo estação (SoftAP de provisioning
 * ativo, ainda reconectando, etc.). */
int8_t nb_wifi_setup_shell_get_rssi(void);

#ifdef __cplusplus
}
#endif

#endif
