#ifndef NB_TOUCH_SERVICE_SHELL_H
#define NB_TOUCH_SERVICE_SHELL_H

#include <stdbool.h>

#include "esp_err.h"
#include "touch_service.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Casca do touch_service (S3.1): inicializa o touch_hal (calibra o
 * baseline), guarda o `nb_touch_service_t` e repassa a leitura periódica
 * pro núcleo. Callback one-shot pros eventos -- sem event_bus ainda
 * (mesma regra do event_bus/tiny_fsm/idle_engine/emotion_core): só nasce
 * quando o reflex_engine (S3.2) precisar consumir de verdade.
 */

typedef void (*nb_touch_service_event_cb_t)(nb_touch_event_t event);

esp_err_t nb_touch_service_shell_init(void);

/* Chamar a cada ~20ms (50Hz, mesma cadência do v1). Lê o touch_hal e
 * avança o núcleo; dispara o callback registrado se um evento ocorrer. */
void nb_touch_service_shell_update(uint32_t dt_ms);

void nb_touch_service_shell_set_event_cb(nb_touch_service_event_cb_t cb);
void nb_touch_service_shell_set_sensitivity(float factor);
void nb_touch_service_shell_set_sleeping(bool sleeping);
bool nb_touch_service_shell_is_pressed(void);
uint32_t nb_touch_service_shell_get_duration_ms(void);

#ifdef __cplusplus
}
#endif

#endif
