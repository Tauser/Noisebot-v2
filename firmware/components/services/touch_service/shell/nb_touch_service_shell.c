#include "nb_touch_service_shell.h"

#include <string.h>

#include "nb_touch_hal_shell.h"
#include "nb_event_bus_shell.h"

#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "touch_service";
static nb_touch_service_t s_svc;
static nb_touch_service_event_cb_t s_event_cb;
static bool s_initialized;

static const char *event_name(nb_touch_event_t event)
{
    switch (event) {
    case NB_TOUCH_EVENT_TAP:
        return "TAP";
    case NB_TOUCH_EVENT_LONG_PRESS:
        return "LONG_PRESS";
    case NB_TOUCH_EVENT_SUSTAINED:
        return "SUSTAINED";
    case NB_TOUCH_EVENT_WAKE:
        return "WAKE";
    default:
        return "UNKNOWN";
    }
}

esp_err_t nb_touch_service_shell_init(void)
{
    if (s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t baseline = 0;
    esp_err_t err = nb_touch_hal_shell_init(&baseline);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "touch_hal init falhou: %s", esp_err_to_name(err));
        return err;
    }

    nb_touch_service_init(&s_svc, baseline);
    s_initialized = true;
    ESP_LOGI(TAG, "touch_service OK -- baseline=%u thr_on=%u thr_off=%u sens=%.2f",
             (unsigned)s_svc.baseline, (unsigned)s_svc.threshold_on,
             (unsigned)s_svc.threshold_off, (double)s_svc.sensitivity);
    return ESP_OK;
}

void nb_touch_service_shell_update(uint32_t dt_ms)
{
    if (!s_initialized) {
        return;
    }

    uint32_t raw = 0;
    if (nb_touch_hal_shell_read(&raw) != ESP_OK) {
        return;
    }

    nb_touch_event_t evt;
    if (nb_touch_service_tick(&s_svc, raw, dt_ms, &evt)) {
        ESP_LOGI(TAG, "event=%s raw=%u fraw=%u baseline=%u thr_on=%u", event_name(evt),
                 (unsigned)s_svc.last_raw, (unsigned)s_svc.filtered_raw, (unsigned)s_svc.baseline,
                 (unsigned)s_svc.threshold_on);
        if (s_event_cb != NULL) {
            s_event_cb(evt);
        }

        /* S3.2: publica no event_bus pro reflex_engine consumir --
         * timestamp real (esp_timer, us -> ms) é o que vira a métrica de
         * latência estímulo->reação (budget < 80ms p95, QUALITY.md). */
        nb_touch_event_payload_t payload = {
            .event = evt,
            .duration_ms = nb_touch_service_get_duration_ms(&s_svc),
        };
        nb_event_t bus_event = {
            .type = NB_EVENT_TYPE_TOUCH,
            .priority = NB_EVENT_PRIORITY_NORMAL,
            .timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000),
            .payload_len = (uint8_t)sizeof(payload),
        };
        memcpy(bus_event.payload, &payload, sizeof(payload));
        nb_event_bus_status_t bus_status = nb_event_bus_shell_publish(&bus_event);
        if (bus_status != NB_EVENT_BUS_OK) {
            ESP_LOGW(TAG, "event_bus publish falhou (status=%d)", (int)bus_status);
        }
    }
}

void nb_touch_service_shell_set_event_cb(nb_touch_service_event_cb_t cb)
{
    s_event_cb = cb;
}

void nb_touch_service_shell_set_sensitivity(float factor)
{
    if (!s_initialized) {
        return;
    }
    nb_touch_service_set_sensitivity(&s_svc, factor);
}

void nb_touch_service_shell_set_sleeping(bool sleeping)
{
    if (!s_initialized) {
        return;
    }
    nb_touch_service_set_sleeping(&s_svc, sleeping);
}

bool nb_touch_service_shell_is_pressed(void)
{
    return s_initialized && nb_touch_service_is_pressed(&s_svc);
}

uint32_t nb_touch_service_shell_get_duration_ms(void)
{
    return s_initialized ? nb_touch_service_get_duration_ms(&s_svc) : 0u;
}
