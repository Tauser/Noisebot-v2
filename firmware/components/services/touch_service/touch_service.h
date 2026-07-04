#ifndef NB_TOUCH_SERVICE_H
#define NB_TOUCH_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro do touch_service (S3.1, camada L3). Porte (reescrito, não
 * copiado) do touch_service do v1 — mesma calibração validada em produto:
 * threshold com histerese, debounce de entrada/saída, EMA de sinal,
 * rejeição de proximidade, recalibração lenta de baseline com proteção
 * contra poisoning, e auto-recalibração de emergência se preso em
 * SUSTAINED por drift de baseline.
 *
 * Sem ESP-IDF/FreeRTOS: leitura bruta e dt_ms são injetados via
 * nb_touch_service_tick(). A casca (shell/) só chama o HAL periodicamente
 * e repassa o valor.
 */

#define NB_TOUCH_SERVICE_NOISE_WINDOW 12u

typedef enum {
    NB_TOUCH_EVENT_TAP = 0,
    NB_TOUCH_EVENT_LONG_PRESS,
    NB_TOUCH_EVENT_SUSTAINED,
    NB_TOUCH_EVENT_WAKE,
} nb_touch_event_t;

typedef enum {
    NB_TOUCH_STATE_IDLE = 0,
    NB_TOUCH_STATE_TOUCHING,
    NB_TOUCH_STATE_LONG_PRESSING,
    NB_TOUCH_STATE_SUSTAINED_ACTIVE,
} nb_touch_state_t;

typedef struct {
    nb_touch_state_t state;
    uint32_t press_ms;

    float sensitivity;
    uint32_t baseline;
    uint32_t threshold_on;
    uint32_t threshold_off;

    uint8_t debounce_on;
    uint8_t debounce_off;

    float filtered_raw;

    uint32_t uptime_ms;
    bool boot_stable;

    uint32_t noise_history[NB_TOUCH_SERVICE_NOISE_WINDOW];
    uint8_t noise_idx;
    bool noise_history_full;

    bool sleeping;
    uint32_t last_raw;
    uint32_t stuck_ms;
} nb_touch_service_t;

/* Inicializa com o baseline calibrado pelo touch_hal e sensibilidade
 * padrão (0.20 = 20%, VISUAL.md/ROADMAP.md "calibração do v1 2.2A"). */
void nb_touch_service_init(nb_touch_service_t *svc, uint32_t baseline);

/* Ajusta a sensibilidade, clampada em [0.002, 1.0] -- o piso baixo existe
 * porque o pad de cobre do v1 tem variação de sinal bem menor que um pad
 * comercial (~1% vs 20-50%), ver nota herdada no núcleo. */
void nb_touch_service_set_sensitivity(nb_touch_service_t *svc, float factor);

/* Enquanto true, toque dispara WAKE em vez de TAP. */
void nb_touch_service_set_sleeping(nb_touch_service_t *svc, bool sleeping);

/* Avança dt_ms com a leitura bruta atual. Retorna true e escreve em
 * out_event se um evento one-shot disparou nesta chamada (nunca mais de
 * um por chamada). Chamar a cada ~20ms (50Hz, mesma cadência do v1). */
bool nb_touch_service_tick(nb_touch_service_t *svc, uint32_t raw, uint32_t dt_ms,
                           nb_touch_event_t *out_event);

bool nb_touch_service_is_pressed(const nb_touch_service_t *svc);
uint32_t nb_touch_service_get_duration_ms(const nb_touch_service_t *svc);

#ifdef __cplusplus
}
#endif

#endif
