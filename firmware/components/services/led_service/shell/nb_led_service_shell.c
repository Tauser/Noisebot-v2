#include "nb_led_service_shell.h"

#include "esp_log.h"
#include "nb_led_hal_shell.h"

static const char *TAG = "led_service";
static nb_led_service_t s_svc;
static bool s_initialized;

esp_err_t nb_led_service_shell_init(void) {
    if (s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = nb_led_hal_shell_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "led_hal init falhou: %s", esp_err_to_name(err));
        return err;
    }
    nb_led_service_init(&s_svc);
    s_initialized = true;
    return ESP_OK;
}

void nb_led_service_shell_tick(nb_fsm_state_t state, uint32_t dt_ms) {
    if (!s_initialized) {
        return;
    }
    nb_led_service_set_state(&s_svc, state);

    nb_led_frame_t frame;
    const bool dirty = nb_led_service_tick(&s_svc, dt_ms, &frame);
    if (!dirty) {
        return;
    }

    for (uint32_t i = 0; i < NB_LED_SERVICE_COUNT; ++i) {
        nb_led_hal_shell_set_pixel(i, frame.pixels[i].r, frame.pixels[i].g, frame.pixels[i].b);
    }
    esp_err_t err = nb_led_hal_shell_refresh();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "refresh falhou: %s", esp_err_to_name(err));
    }
}

void nb_led_service_shell_trigger_touch(void) {
    if (!s_initialized) {
        return;
    }
    nb_led_service_trigger_touch(&s_svc);
}

void nb_led_service_shell_set_brightness_scale(float scale) {
    if (!s_initialized) {
        return;
    }
    nb_led_service_set_brightness_scale(&s_svc, scale);
}
