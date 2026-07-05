#ifndef NB_EVENT_BUS_SHELL_H
#define NB_EVENT_BUS_SHELL_H

#include "esp_err.h"
#include "event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Casca do event_bus (S3.2, primeiro consumidor real -- reflex_engine).
 * Instância única, protegida por critical section (o núcleo não assume
 * concorrência interna, README do componente): publish() roda na task de
 * touch_service a 50Hz, poll() roda na task de face/reflex a ~30Hz.
 */

esp_err_t nb_event_bus_shell_init(void);

nb_event_bus_status_t nb_event_bus_shell_publish(const nb_event_t *event);
nb_event_bus_status_t nb_event_bus_shell_poll(nb_event_t *out_event);
void nb_event_bus_shell_get_stats(nb_event_bus_stats_t *out_stats);

#ifdef __cplusplus
}
#endif

#endif
