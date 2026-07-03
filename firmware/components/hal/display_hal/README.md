# display_hal

Primeiro componente em `firmware/components/hal/` (camada L0 de
`ARCHITECTURE.md`). Núcleo C17 puro (`display_hal.c/.h`): só o bookkeeping
de double buffer (índice front/back, swap) — sem FreeRTOS, SPI, DMA nem
ESP-IDF. Os ponteiros dos dois framebuffers são injetados pelo chamador.

Casca (`shell/nb_display_hal_shell.c/.h`): ST7789 via `esp_lcd` nativo do
ESP-IDF (`esp_lcd_panel_io_spi` + `esp_lcd_new_panel_st7789`), **C17 puro,
sem LovyanGFX/C++ nesta camada** — decisão explícita para respeitar
`CLAUDE.md` ("C17 em tudo, exceto o renderer"). LovyanGFX (C++, atrás de
`extern "C"`) fica isolado no renderer paramétrico (S2.2), que desenha nos
buffers alocados aqui e chama `nb_display_hal_shell_flush_and_swap()`.

Pinos (SPI2, `HARDWARE.md` — ainda marcado `SPIKE`, S0.1/S0.4 pendentes):

| Sinal | GPIO |
| --- | --- |
| CS | 10 |
| MOSI | 11 |
| SCLK | 12 |
| DC | 14 |
| MISO | não conectado (reservado câmera) |
| RST | nenhum pino — reset via comando `SWRESET` |
| Backlight | sem controle — 3.3V fixo |

Framebuffers (320×240 RGB565, ~150 KB cada) sempre em PSRAM
(`heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`) — nunca em SRAM
(`ARCHITECTURE.md` §6).

`nb_display_hal_shell_draw_test_pattern()` é um padrão de bring-up (bandas
de cor horizontais) para confirmar visualmente que o display está
respondendo — não é o renderer real.

**Pendente (fora do escopo desta fatia, ver `docs/ROADMAP.md` §S2.1):** soak
de 30 fps por 1h com medição de artefato/SRAM via `.map`; esse teste
prolongado fica registrado como próximo passo depois da confirmação visual
inicial.
