#include "nb_app_config_shell.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "nvs_flash.h"

#define NB_APP_CONFIG_NVS_NAMESPACE "nb_cfg"

static nb_config_t s_config;
static nvs_handle_t s_nvs_handle;
static SemaphoreHandle_t s_mutex;

static void nb_app_config_shell_load(void)
{
    for (nb_config_key_t key = 0; key < NB_CONFIG_KEY_COUNT; ++key) {
        const nb_config_descriptor_t *descriptor = nb_config_get_descriptor(key);

        if (descriptor == NULL) {
            continue;
        }

        if (descriptor->type == NB_CONFIG_TYPE_U32) {
            uint32_t value;
            if (nvs_get_u32(s_nvs_handle, descriptor->name, &value) == ESP_OK) {
                nb_config_set_u32(&s_config, key, value);
            }
        } else if (descriptor->type == NB_CONFIG_TYPE_I32) {
            int32_t value;
            if (nvs_get_i32(s_nvs_handle, descriptor->name, &value) == ESP_OK) {
                nb_config_set_i32(&s_config, key, value);
            }
        }
    }
}

esp_err_t nb_app_config_shell_init(void)
{
    esp_err_t err;

    if (s_mutex != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_open(NB_APP_CONFIG_NVS_NAMESPACE, NVS_READWRITE, &s_nvs_handle);
    if (err != ESP_OK) {
        return err;
    }

    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        nvs_close(s_nvs_handle);
        return ESP_ERR_NO_MEM;
    }

    nb_config_init(&s_config);
    nb_app_config_shell_load();
    return ESP_OK;
}

bool nb_app_config_shell_get_u32(nb_config_key_t key, uint32_t *out_value)
{
    bool ok;

    if (s_mutex == NULL) {
        return false;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    ok = nb_config_get_u32(&s_config, key, out_value);
    xSemaphoreGive(s_mutex);

    return ok;
}

bool nb_app_config_shell_set_u32(nb_config_key_t key, uint32_t value)
{
    uint32_t previous = 0;
    bool ok;

    if (s_mutex == NULL) {
        return false;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    nb_config_get_u32(&s_config, key, &previous);
    ok = nb_config_set_u32(&s_config, key, value);
    if (ok && previous != value) {
        const nb_config_descriptor_t *descriptor = nb_config_get_descriptor(key);
        nvs_set_u32(s_nvs_handle, descriptor->name, value);
        nvs_commit(s_nvs_handle);
    }
    xSemaphoreGive(s_mutex);

    return ok;
}

bool nb_app_config_shell_get_i32(nb_config_key_t key, int32_t *out_value)
{
    bool ok;

    if (s_mutex == NULL) {
        return false;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    ok = nb_config_get_i32(&s_config, key, out_value);
    xSemaphoreGive(s_mutex);

    return ok;
}

bool nb_app_config_shell_set_i32(nb_config_key_t key, int32_t value)
{
    int32_t previous = 0;
    bool ok;

    if (s_mutex == NULL) {
        return false;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    nb_config_get_i32(&s_config, key, &previous);
    ok = nb_config_set_i32(&s_config, key, value);
    if (ok && previous != value) {
        const nb_config_descriptor_t *descriptor = nb_config_get_descriptor(key);
        nvs_set_i32(s_nvs_handle, descriptor->name, value);
        nvs_commit(s_nvs_handle);
    }
    xSemaphoreGive(s_mutex);

    return ok;
}
