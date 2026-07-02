#include "nb_logger_shell.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static nb_logger_t s_logger;
static SemaphoreHandle_t s_mutex;

static uint32_t nb_logger_shell_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

esp_err_t nb_logger_shell_init(nb_log_level_t min_level)
{
    if (s_mutex != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    nb_logger_init(&s_logger, min_level);
    return ESP_OK;
}

bool nb_logger_shell_log(nb_log_level_t level, const char *module, const char *message)
{
    bool logged;

    if (s_mutex == NULL) {
        return false;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    logged = nb_logger_log(&s_logger, nb_logger_shell_now_ms(), level, module, message);
    xSemaphoreGive(s_mutex);

    return logged;
}

void nb_logger_shell_set_level(nb_log_level_t min_level)
{
    if (s_mutex == NULL) {
        return;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    nb_logger_set_level(&s_logger, min_level);
    xSemaphoreGive(s_mutex);
}

void nb_logger_shell_get_stats(nb_logger_stats_t *out_stats)
{
    if (s_mutex == NULL || out_stats == NULL) {
        return;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    nb_logger_get_stats(&s_logger, out_stats);
    xSemaphoreGive(s_mutex);
}

size_t nb_logger_shell_copy_all(nb_log_entry_t *out_entries, size_t max_entries)
{
    size_t copied;

    if (s_mutex == NULL) {
        return 0u;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    copied = nb_logger_copy_all(&s_logger, out_entries, max_entries);
    xSemaphoreGive(s_mutex);

    return copied;
}

size_t nb_logger_shell_drain_since(uint32_t *inout_last_seq, nb_log_entry_t *out_entries,
                                   size_t max_entries, bool *out_gap_detected)
{
    size_t copied;

    if (s_mutex == NULL) {
        return 0u;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    copied = nb_logger_drain_since(&s_logger, inout_last_seq, out_entries, max_entries,
                                    out_gap_detected);
    xSemaphoreGive(s_mutex);

    return copied;
}
