#include "nb_led_hal_shell.h"

#include "esp_log.h"
#include "led_strip.h"
#include "nb_hw_config.h"

static const char *TAG = "led_hal";
static led_strip_handle_t s_strip;
static bool s_initialized;

esp_err_t nb_led_hal_shell_init(void) {
    if (s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    led_strip_config_t strip_cfg = {
        .strip_gpio_num = NB_HW_GPIO_WS2812,
        .max_leds = NB_HW_LED_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        },
    };
    led_strip_rmt_config_t rmt_cfg = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 0,
        .flags = {
            .with_dma = false,
        },
    };

    esp_err_t err = led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "led_strip_new_rmt_device falhou: %s", esp_err_to_name(err));
        return err;
    }

    err = led_strip_clear(s_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "clear inicial falhou: %s", esp_err_to_name(err));
        return err;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "led_hal OK -- GPIO%d, %u LEDs", NB_HW_GPIO_WS2812, (unsigned)NB_HW_LED_COUNT);
    return ESP_OK;
}

esp_err_t nb_led_hal_shell_set_pixel(uint32_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (!s_initialized || index >= NB_HW_LED_COUNT) {
        return ESP_ERR_INVALID_STATE;
    }
    return led_strip_set_pixel(s_strip, index, r, g, b);
}

esp_err_t nb_led_hal_shell_refresh(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    return led_strip_refresh(s_strip);
}
