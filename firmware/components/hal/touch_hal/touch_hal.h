#ifndef NB_TOUCH_HAL_H
#define NB_TOUCH_HAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro do touch_hal (S3.1, camada L0). Sem ESP-IDF: só a média
 * usada para calcular o baseline de calibração de boot -- a casca
 * (shell/) coleta as amostras via o periférico touch nativo do ESP-IDF
 * (GPIO2/TOUCH2, `HARDWARE.md`) e chama esta função.
 */

/* Média das amostras coletadas na calibração de boot. samples==NULL ou
 * count==0 retorna 0 (baseline não calibrado). */
uint32_t nb_touch_hal_compute_baseline(const uint32_t *samples, size_t count);

#ifdef __cplusplus
}
#endif

#endif
