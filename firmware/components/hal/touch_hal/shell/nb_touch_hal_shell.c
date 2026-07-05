#include "nb_touch_hal_shell.h"

#include "touch_hal.h"

#include <stdbool.h>

#include "driver/touch_sens.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nb_hw_config.h"

#define NB_TOUCH_HAL_SHELL_SETTLE_MS 200u
#define NB_TOUCH_HAL_SHELL_CALIB_SAMPLES 10u
#define NB_TOUCH_HAL_SHELL_CALIB_SAMPLE_INTERVAL_MS 10u /* 10 amostras em 100ms */

static const char *TAG = "touch_hal";
static touch_sensor_handle_t s_sens_handle;
static touch_channel_handle_t s_chan_handle;
static bool s_initialized;

esp_err_t nb_touch_hal_shell_init(uint32_t *out_baseline)
{
    if (s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (out_baseline == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    touch_sensor_sample_config_t sample_cfg[TOUCH_SAMPLE_CFG_NUM] = {
        TOUCH_SENSOR_V2_DEFAULT_SAMPLE_CONFIG(500, TOUCH_VOLT_LIM_L_0V5, TOUCH_VOLT_LIM_H_2V2),
    };
    touch_sensor_config_t sens_cfg = TOUCH_SENSOR_DEFAULT_BASIC_CONFIG(1, sample_cfg);
    esp_err_t err = touch_sensor_new_controller(&sens_cfg, &s_sens_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "new_controller falhou: %s", esp_err_to_name(err));
        return err;
    }

    /* active_thresh do canal fica alto e inerte de propósito -- a detecção
     * real de toque roda no touch_service (S3.1, núcleo puro), não no
     * mecanismo de ativação de hardware. Só lemos o dado suavizado. */
    touch_channel_config_t chan_cfg = {
        .active_thresh = {2000},
        .charge_speed = TOUCH_CHARGE_SPEED_7,
        .init_charge_volt = TOUCH_INIT_CHARGE_VOLT_DEFAULT,
    };
    err = touch_sensor_new_channel(s_sens_handle, NB_HW_TOUCH_CHANNEL, &chan_cfg,
                                   &s_chan_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "new_channel falhou: %s", esp_err_to_name(err));
        return err;
    }

    err = touch_sensor_enable(s_sens_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "enable falhou: %s", esp_err_to_name(err));
        return err;
    }
    err = touch_sensor_start_continuous_scanning(s_sens_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "start_continuous_scanning falhou: %s", esp_err_to_name(err));
        return err;
    }

    /* Settle: não tocar o sensor durante este período (touch_hal.h). */
    vTaskDelay(pdMS_TO_TICKS(NB_TOUCH_HAL_SHELL_SETTLE_MS));

    uint32_t samples[NB_TOUCH_HAL_SHELL_CALIB_SAMPLES];
    for (uint32_t i = 0; i < NB_TOUCH_HAL_SHELL_CALIB_SAMPLES; ++i) {
        uint32_t raw = 0;
        err = touch_channel_read_data(s_chan_handle, TOUCH_CHAN_DATA_TYPE_SMOOTH, &raw);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "read falhou na calibracao: %s", esp_err_to_name(err));
            return err;
        }
        samples[i] = raw;
        vTaskDelay(pdMS_TO_TICKS(NB_TOUCH_HAL_SHELL_CALIB_SAMPLE_INTERVAL_MS));
    }

    *out_baseline = nb_touch_hal_compute_baseline(samples, NB_TOUCH_HAL_SHELL_CALIB_SAMPLES);
    s_initialized = true;
    ESP_LOGI(TAG, "touch_hal OK -- GPIO%d canal %d, baseline=%u", NB_HW_GPIO_TOUCH2,
             NB_HW_TOUCH_CHANNEL, (unsigned)*out_baseline);
    return ESP_OK;
}

esp_err_t nb_touch_hal_shell_read(uint32_t *raw)
{
    if (!s_initialized || raw == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    return touch_channel_read_data(s_chan_handle, TOUCH_CHAN_DATA_TYPE_SMOOTH, raw);
}

void nb_touch_hal_shell_deinit(void)
{
    if (!s_initialized) {
        return;
    }
    touch_sensor_stop_continuous_scanning(s_sens_handle);
    touch_sensor_disable(s_sens_handle);
    touch_sensor_del_channel(s_chan_handle);
    touch_sensor_del_controller(s_sens_handle);
    s_initialized = false;
}
