#include "nb_schedule_core_shell.h"

#include "esp_log.h"
#include "nb_circadian_core_shell.h"
#include "nvs.h"
#include "nvs_flash.h"

#define NB_SCHEDULE_SHELL_NVS_NAMESPACE "nb_sched"
#define NB_SCHEDULE_SHELL_NVS_KEY "timers"

static const char *TAG = "schedule_core";
static nb_schedule_core_t s_core;
static bool s_initialized;

static void persist(void) {
    nvs_handle_t handle;
    if (nvs_open(NB_SCHEDULE_SHELL_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        return;
    }
    if (nvs_set_blob(handle, NB_SCHEDULE_SHELL_NVS_KEY, &s_core, sizeof(s_core)) == ESP_OK) {
        nvs_commit(handle);
    }
    nvs_close(handle);
}

void nb_schedule_core_shell_init(void) {
    nb_schedule_core_init(&s_core);

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init falhou (%s) -- seguindo sem persistencia",
                 esp_err_to_name(err));
        s_initialized = true;
        return;
    }

    nvs_handle_t handle;
    if (nvs_open(NB_SCHEDULE_SHELL_NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        size_t len = sizeof(s_core);
        esp_err_t get_err = nvs_get_blob(handle, NB_SCHEDULE_SHELL_NVS_KEY, &s_core, &len);
        if (get_err == ESP_OK && len == sizeof(s_core)) {
            ESP_LOGI(TAG, "restaurado do NVS -- %u timers", (unsigned)nb_schedule_core_count(&s_core));
        } else if (get_err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "restauracao NVS falhou (%s) -- comecando vazio", esp_err_to_name(get_err));
            nb_schedule_core_init(&s_core);
        }
        nvs_close(handle);
    }

    s_initialized = true;
}

void nb_schedule_core_shell_handle_set(uint32_t timer_id, uint64_t fire_at_unix_ms) {
    if (!s_initialized) {
        return;
    }
    const nb_schedule_status_t status =
        nb_schedule_core_create(&s_core, timer_id, fire_at_unix_ms, NULL);
    if (status == NB_SCHEDULE_STATUS_OK) {
        persist();
        ESP_LOGI(TAG, "TIMER_SET aplicado -- id=%u fire_at=%llu", (unsigned)timer_id,
                 (unsigned long long)fire_at_unix_ms);
    } else {
        ESP_LOGW(TAG, "TIMER_SET rejeitado (status=%d) -- id=%u", (int)status, (unsigned)timer_id);
    }
}

void nb_schedule_core_shell_handle_cancel(uint32_t timer_id) {
    if (!s_initialized) {
        return;
    }
    if (nb_schedule_core_cancel(&s_core, timer_id) == NB_SCHEDULE_STATUS_OK) {
        persist();
        ESP_LOGI(TAG, "TIMER_CANCEL aplicado -- id=%u", (unsigned)timer_id);
    }
}

uint32_t nb_schedule_core_shell_tick(uint32_t *out_fired_ids, uint32_t max_fired) {
    if (!s_initialized) {
        return 0u;
    }
    const uint64_t now_unix_ms = nb_circadian_core_shell_now_unix_ms();
    const uint32_t count = nb_schedule_core_tick(&s_core, now_unix_ms, out_fired_ids, max_fired);
    if (count > 0u) {
        for (uint32_t i = 0; i < count; ++i) {
            ESP_LOGI(TAG, "disparou -- id=%u now_unix_ms=%llu", (unsigned)out_fired_ids[i],
                     (unsigned long long)now_unix_ms);
        }
        persist();
    }
    return count;
}
