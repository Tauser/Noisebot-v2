#include "nb_wifi_setup_shell.h"

#include <stdio.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_softap.h"

#define NB_WIFI_SETUP_POP "nb2setup"
#define NB_WIFI_SETUP_SERVICE_NAME_MAX 32u

static const char *TAG = "wifi_setup";
static nb_wifi_setup_t s_setup;

static void nb_wifi_setup_shell_event_handler(void *arg, esp_event_base_t event_base,
                                              int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base != WIFI_PROV_EVENT) {
        return;
    }

    switch (event_id) {
    case WIFI_PROV_START:
        nb_wifi_setup_begin(&s_setup);
        ESP_LOGI(TAG, "provisioning iniciado");
        break;
    case WIFI_PROV_CRED_RECV:
        ESP_LOGI(TAG, "credenciais recebidas");
        break;
    case WIFI_PROV_CRED_FAIL:
        nb_wifi_setup_fail(&s_setup);
        ESP_LOGE(TAG, "falha ao aplicar credenciais");
        break;
    case WIFI_PROV_CRED_SUCCESS:
        /* SSID/senha já validados pelo próprio AP (conexão estabelecida);
         * marca como provisionado sem repetir a validação de formato aqui. */
        s_setup.state = NB_WIFI_SETUP_STATE_PROVISIONED;
        ESP_LOGI(TAG, "credenciais aplicadas com sucesso");
        break;
    case WIFI_PROV_END:
        ESP_LOGI(TAG, "provisioning encerrado");
        wifi_prov_mgr_deinit();
        break;
    default:
        break;
    }
}

static void nb_wifi_setup_shell_wifi_event_handler(void *arg, esp_event_base_t event_base,
                                                   int32_t event_id, void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        /* esp_wifi_start() em modo estação não conecta sozinho — é preciso
         * pedir explicitamente, e de novo a cada desconexão (troca de AP,
         * sinal fraco, reboot do roteador etc.). Sem isso a estação nunca
         * associa e todo o resto (mind_link) fica "network unreachable"
         * pra sempre, sem nunca dar erro óbvio disso. */
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "desconectado do AP, tentando reconectar");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "IP obtido: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

static void nb_wifi_setup_shell_derive_service_name(char *out_name, size_t out_size)
{
    uint8_t mac[6] = {0};

    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(out_name, out_size, "NoiseBot2-%02X%02X", mac[4], mac[5]);
}

esp_err_t nb_wifi_setup_shell_init(void)
{
    esp_err_t err;
    bool provisioned = false;
    char service_name[NB_WIFI_SETUP_SERVICE_NAME_MAX];
    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    wifi_prov_mgr_config_t prov_cfg = {
        .scheme = wifi_prov_scheme_softap,
        .scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE,
        .app_event_handler = WIFI_PROV_EVENT_HANDLER_NONE,
    };

    nb_wifi_setup_init(&s_setup);

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        return err;
    }

    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    err = esp_wifi_init(&wifi_init_cfg);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID,
                                     &nb_wifi_setup_shell_event_handler, NULL);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                     &nb_wifi_setup_shell_wifi_event_handler, NULL);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                     &nb_wifi_setup_shell_wifi_event_handler, NULL);
    if (err != ESP_OK) {
        return err;
    }

    err = wifi_prov_mgr_init(prov_cfg);
    if (err != ESP_OK) {
        return err;
    }

    err = wifi_prov_mgr_is_provisioned(&provisioned);
    if (err != ESP_OK) {
        return err;
    }

    if (provisioned) {
        s_setup.state = NB_WIFI_SETUP_STATE_PROVISIONED;
        wifi_prov_mgr_deinit();
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_LOGI(TAG, "ja provisionado, conectando em modo estacao");
        return ESP_OK;
    }

    nb_wifi_setup_shell_derive_service_name(service_name, sizeof(service_name));
    ESP_LOGI(TAG, "nao provisionado: SoftAP '%s', PoP '%s'", service_name, NB_WIFI_SETUP_POP);

    return wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, NB_WIFI_SETUP_POP,
                                            service_name, NULL);
}

nb_wifi_setup_state_t nb_wifi_setup_shell_get_state(void)
{
    return nb_wifi_setup_get_state(&s_setup);
}

int8_t nb_wifi_setup_shell_get_rssi(void)
{
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
        return -128;
    }
    return ap_info.rssi;
}
