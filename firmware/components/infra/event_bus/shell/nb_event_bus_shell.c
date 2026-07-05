#include "nb_event_bus_shell.h"

#include "freertos/FreeRTOS.h"

static nb_event_bus_t s_bus;
static bool s_initialized;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

esp_err_t nb_event_bus_shell_init(void) {
    if (s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    nb_event_bus_init(&s_bus);
    s_initialized = true;
    return ESP_OK;
}

nb_event_bus_status_t nb_event_bus_shell_publish(const nb_event_t *event) {
    if (!s_initialized) {
        return NB_EVENT_BUS_ERR_INVALID_ARG;
    }
    nb_event_bus_status_t status;
    portENTER_CRITICAL(&s_mux);
    status = nb_event_bus_publish(&s_bus, event);
    portEXIT_CRITICAL(&s_mux);
    return status;
}

nb_event_bus_status_t nb_event_bus_shell_poll(nb_event_t *out_event) {
    if (!s_initialized) {
        return NB_EVENT_BUS_ERR_INVALID_ARG;
    }
    nb_event_bus_status_t status;
    portENTER_CRITICAL(&s_mux);
    status = nb_event_bus_poll(&s_bus, out_event);
    portEXIT_CRITICAL(&s_mux);
    return status;
}

void nb_event_bus_shell_get_stats(nb_event_bus_stats_t *out_stats) {
    if (!s_initialized) {
        return;
    }
    portENTER_CRITICAL(&s_mux);
    nb_event_bus_get_stats(&s_bus, out_stats);
    portEXIT_CRITICAL(&s_mux);
}
