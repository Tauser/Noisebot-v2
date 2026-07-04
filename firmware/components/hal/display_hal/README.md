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

**Sincronização DMA:** `esp_lcd_panel_draw_bitmap()` no SPI é assíncrono
(enfileira o DMA e retorna antes de os pixels saírem). `flush_and_swap()`
usa um semáforo liberado no callback `on_color_trans_done` para esperar a
transferência anterior antes de reenfileirar e trocar os buffers — sem essa
barreira a task de render sobrescreve o framebuffer enquanto o DMA ainda o
lê, misturando dois frames nas bandas (flicker). O double buffer não basta
sozinho quando o loop de render é mais rápido que o tempo de transmissão de
um frame (~61 ms @ 20 MHz; ~31 ms @ 40 MHz).

**Coerência de cache PSRAM:** o `esp_lcd_panel_io_spi` não seta
`SPI_TRANS_DMA_USE_PSRAM` nas transações, então o `spi_master` não faz o
`cache_msync` automático — o DMA lia da PSRAM dado ainda não escrito de
volta da cache, corrompendo bits de cor de forma intermitente (vermelho→
laranja, azul→roxo), independente de clock e fiação. `flush_and_swap()`
chama `esp_cache_msync(..., ESP_CACHE_MSYNC_FLAG_DIR_C2M)` no back buffer
antes de cada `draw_bitmap`. Requer buffer alinhado à linha de cache (ver
alocação em `shell_init`).

**Pendente (fora do escopo desta fatia, ver `docs/ROADMAP.md` §S2.1):** soak
de 30 fps por 1h com medição de artefato/SRAM via `.map`; esse teste
prolongado fica registrado como próximo passo depois da confirmação visual
inicial.

**Orientação (bug real, achado no bring-up do S2.2):** `swap_xy(true)`
sozinho deixava a imagem de cabeça para baixo nesta montagem. O padrão de
barras horizontais do bring-up do S2.1 não denunciava isso — uma faixa
invertida verticalmente ainda parece um conjunto válido de faixas
horizontais; só ficou visível com conteúdo assimétrico em cima/embaixo
(os olhos do renderer, S2.2). Depois do `swap_xy` (MV=1), os parâmetros
`mirror_x`/`mirror_y` do `esp_lcd` mapeiam pros eixos físicos já trocados;
`mirror(false, true)` não corrigiu, `mirror(true, false)` corrigiu —
achado por teste direto em bancada, não por dedução do datasheet.
