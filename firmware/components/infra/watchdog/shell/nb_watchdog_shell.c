#include "nb_watchdog_shell.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#define NB_WATCHDOG_NVS_NAMESPACE "nb_wdog"
#define NB_WATCHDOG_NVS_LAST_CAUSE "last_cause"
#define NB_WATCHDOG_NVS_LAST_RESET "last_reset"
#define NB_WATCHDOG_MAIN_TASK_NAME "app_main"

static const char *TAG = "watchdog";

static nb_watchdog_t s_watchdog;
static uint8_t s_main_task_id;
static SemaphoreHandle_t s_mutex;
static bool s_initialized;

static uint32_t nb_watchdog_shell_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static nb_watchdog_reset_cause_t nb_watchdog_shell_map_reset_reason(esp_reset_reason_t reason)
{
    if (reason == ESP_RST_TASK_WDT) {
        return NB_WATCHDOG_RESET_CAUSE_TASK_TIMEOUT;
    }
    if (reason == ESP_RST_INT_WDT) {
        return NB_WATCHDOG_RESET_CAUSE_HW_WDT;
    }
    if (reason == ESP_RST_WDT) {
        return NB_WATCHDOG_RESET_CAUSE_UNKNOWN_WDT;
    }
    return NB_WATCHDOG_RESET_CAUSE_NONE;
}

static esp_err_t nb_watchdog_shell_store_reset_cause(esp_reset_reason_t reason,
                                                     nb_watchdog_reset_cause_t cause)
{
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_open(NB_WATCHDOG_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_u32(handle, NB_WATCHDOG_NVS_LAST_RESET, (uint32_t)reason);
    if (err == ESP_OK) {
        err = nvs_set_u32(handle, NB_WATCHDOG_NVS_LAST_CAUSE, (uint32_t)cause);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    return err;
}

esp_err_t nb_watchdog_shell_init(uint32_t timeout_ms)
{
    esp_task_wdt_config_t config = {
        .timeout_ms = timeout_ms,
        .idle_core_mask = (1u << portNUM_PROCESSORS) - 1u,
        .trigger_panic = true,
    };
    esp_reset_reason_t reset_reason;
    nb_watchdog_reset_cause_t reset_cause;
    esp_err_t err;

    if (timeout_ms == 0u) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    nb_watchdog_init(&s_watchdog);
    reset_reason = esp_reset_reason();
    reset_cause = nb_watchdog_shell_map_reset_reason(reset_reason);
    nb_watchdog_set_last_reset_cause(&s_watchdog, reset_cause);

    err = nb_watchdog_shell_store_reset_cause(reset_reason, reset_cause);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_task_wdt_init(&config);
    if (err == ESP_ERR_INVALID_STATE) {
        /* ESP-IDF já inicializou a TWDT do sistema (CONFIG_ESP_TASK_WDT_INIT)
         * antes de chegarmos aqui, tipicamente sem panic habilitado. Sem
         * reconfigurar, nossa escolha de timeout/trigger_panic seria
         * silenciosamente ignorada e uma task travada nunca reiniciaria o
         * robô. */
        err = esp_task_wdt_reconfigure(&config);
    }
    if (err != ESP_OK) {
        return err;
    }

    if (esp_task_wdt_status(NULL) != ESP_OK) {
        err = esp_task_wdt_add(NULL);
        if (err != ESP_OK) {
            return err;
        }
    }

    if (nb_watchdog_register_task(&s_watchdog, NB_WATCHDOG_MAIN_TASK_NAME,
                                  nb_watchdog_shell_now_ms(), timeout_ms,
                                  &s_main_task_id) != NB_WATCHDOG_STATUS_OK) {
        return ESP_FAIL;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "TWDT ativo: timeout=%u ms reset_anterior=%d",
             (unsigned)timeout_ms, (int)reset_cause);
    return ESP_OK;
}

esp_err_t nb_watchdog_shell_feed(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (nb_watchdog_feed(&s_watchdog, s_main_task_id, nb_watchdog_shell_now_ms()) !=
        NB_WATCHDOG_STATUS_OK) {
        xSemaphoreGive(s_mutex);
        return ESP_FAIL;
    }
    xSemaphoreGive(s_mutex);

    return esp_task_wdt_reset();
}

nb_watchdog_reset_cause_t nb_watchdog_shell_get_last_reset_cause(void)
{
    nb_watchdog_reset_cause_t cause;

    if (!s_initialized) {
        return NB_WATCHDOG_RESET_CAUSE_NONE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    cause = nb_watchdog_get_last_reset_cause(&s_watchdog);
    xSemaphoreGive(s_mutex);

    return cause;
}
