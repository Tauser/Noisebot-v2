#include "nb_ota_shell.h"

#include "esp_log.h"
#include "esp_ota_ops.h"

static const char *TAG = "ota";

esp_err_t nb_ota_shell_confirm_boot_if_pending(void)
{
    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state = ESP_OTA_IMG_UNDEFINED;
    esp_err_t err;

    if (running_partition == NULL) {
        ESP_LOGE(TAG, "particao OTA em execucao indisponivel");
        return ESP_ERR_NOT_FOUND;
    }

    err = esp_ota_get_state_partition(running_partition, &ota_state);
    if (err == ESP_ERR_NOT_SUPPORTED) {
        ESP_LOGW(TAG, "rollback OTA desabilitado no sdkconfig atual");
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "falha ao ler estado OTA: %s", esp_err_to_name(err));
        return err;
    }

    if (ota_state != ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGI(TAG, "imagem OTA atual sem verificacao pendente (state=%d)",
                 (int)ota_state);
        return ESP_OK;
    }

    err = esp_ota_mark_app_valid_cancel_rollback();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "falha ao confirmar imagem OTA: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "imagem OTA confirmada; rollback cancelado");
    return ESP_OK;
}

esp_err_t nb_ota_shell_rollback_and_reboot(void)
{
    ESP_LOGE(TAG, "imagem OTA invalida; solicitando rollback e reboot");
    return esp_ota_mark_app_invalid_rollback_and_reboot();
}
