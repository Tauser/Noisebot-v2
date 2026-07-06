#ifndef NB_DISPLAY_HAL_SHELL_H
#define NB_DISPLAY_HAL_SHELL_H

#include <stdint.h>

#include "display_hal.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Casca do display_hal (S2.1): ST7789 via `esp_lcd` nativo do ESP-IDF
 * (esp_lcd_panel_io_spi + esp_lcd_new_panel_st7789), C17 puro — sem
 * LovyanGFX nesta camada (decisão registrada em docs/ROADMAP.md §S2.1;
 * LovyanGFX/C++ fica isolado no renderer, S2.2).
 *
 * Pinos (HARDWARE.md, ainda SPIKE — S0.1/S0.4 pendentes): CS=10, MOSI=11,
 * SCLK=12, DC=14; sem MISO, sem RST dedicado (reset via SWRESET), sem
 * controle de backlight (BL fixo em 3.3V).
 */

esp_err_t nb_display_hal_shell_init(void);

uint16_t *nb_display_hal_shell_get_back_buffer(void);

/* Manda o back buffer inteiro (320x240 RGB565) pro painel via DMA e troca
 * front/back no núcleo. */
esp_err_t nb_display_hal_shell_flush_and_swap(void);

/* Manda só o retângulo sujo do back buffer. A casca alinha e fatia a região
 * para manter o staging PSRAM->SRAM pequeno e compatível com cache. */
esp_err_t nb_display_hal_shell_flush_rect_and_swap(nb_display_hal_rect_t rect);

/* Padrão de teste visual (bandas de cor horizontais; `offset_rows` existe
 * para testes manuais de scroll, mas o bring-up padrão usa 0 para deixar a
 * imagem estática e facilitar diagnóstico de cor/sinal). */
void nb_display_hal_shell_draw_test_pattern(uint16_t *buffer, int offset_rows);

#ifdef __cplusplus
}
#endif

#endif
