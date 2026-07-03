#include "nb_mind_link_token_shell.h"

#include <stdbool.h>
#include <string.h>

#include "nvs.h"
#include "nvs_flash.h"

#define NB_MIND_LINK_TOKEN_NVS_NAMESPACE "nbp2_tok"
#define NB_MIND_LINK_TOKEN_NVS_KEY "token"

static uint8_t s_token[NB_MIND_LINK_TOKEN_MAX_LEN];
static size_t s_token_len;
static bool s_initialized;

esp_err_t nb_mind_link_token_shell_init(void)
{
    nvs_handle_t handle;
    esp_err_t err;
    size_t len = sizeof(s_token);

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_open(NB_MIND_LINK_TOKEN_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    s_token_len = 0u;
    err = nvs_get_blob(handle, NB_MIND_LINK_TOKEN_NVS_KEY, s_token, &len);
    if (err == ESP_OK) {
        s_token_len = len;
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return err;
    }

    nvs_close(handle);
    s_initialized = true;
    return ESP_OK;
}

size_t nb_mind_link_token_shell_get(uint8_t *out_token, size_t out_cap)
{
    size_t len;

    if (!s_initialized || out_token == NULL) {
        return 0u;
    }

    len = s_token_len < out_cap ? s_token_len : out_cap;
    memcpy(out_token, s_token, len);
    return len;
}

esp_err_t nb_mind_link_token_shell_set(const uint8_t *token, size_t token_len)
{
    nvs_handle_t handle;
    esp_err_t err;

    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (token_len > NB_MIND_LINK_TOKEN_MAX_LEN || (token == NULL && token_len > 0u)) {
        return ESP_ERR_INVALID_ARG;
    }

    err = nvs_open(NB_MIND_LINK_TOKEN_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_blob(handle, NB_MIND_LINK_TOKEN_NVS_KEY, token, token_len);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    if (err != ESP_OK) {
        return err;
    }

    if (token_len > 0u) {
        memcpy(s_token, token, token_len);
    }
    s_token_len = token_len;
    return ESP_OK;
}
