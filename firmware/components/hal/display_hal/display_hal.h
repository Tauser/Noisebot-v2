#ifndef NB_DISPLAY_HAL_H
#define NB_DISPLAY_HAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro do display_hal (S2.1).
 *
 * Sem FreeRTOS, sem ESP-IDF, sem SPI/DMA: só o bookkeeping de double buffer
 * (qual dos dois framebuffers está "na tela" agora). Os ponteiros dos
 * buffers são injetados pelo chamador — a casca (shell/) é quem aloca os
 * dois em PSRAM e efetivamente empurra bytes pro painel via `esp_lcd`.
 */

#define NB_DISPLAY_HAL_WIDTH 320u
#define NB_DISPLAY_HAL_HEIGHT 240u

typedef struct {
    uint16_t *buffers[2];
    uint8_t front_index; /* 0 ou 1: qual buffer foi mostrado por último */
} nb_display_hal_t;

/* buffer_a/buffer_b devem ter NB_DISPLAY_HAL_WIDTH*NB_DISPLAY_HAL_HEIGHT
 * pixels RGB565 cada (alocação é responsabilidade do chamador). */
void nb_display_hal_init(nb_display_hal_t *hal, uint16_t *buffer_a, uint16_t *buffer_b);

/* Buffer que não está na tela — é nele que o renderer desenha o próximo
 * frame. */
uint16_t *nb_display_hal_get_back_buffer(const nb_display_hal_t *hal);

/* Buffer que foi mostrado no último flush. */
uint16_t *nb_display_hal_get_front_buffer(const nb_display_hal_t *hal);

/* Troca front/back depois que a casca termina de mandar o back buffer pro
 * painel. Não faz I/O nenhum — só o bookkeeping. */
void nb_display_hal_swap(nb_display_hal_t *hal);

#ifdef __cplusplus
}
#endif

#endif
