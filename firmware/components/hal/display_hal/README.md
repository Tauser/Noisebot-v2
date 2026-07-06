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
buffers alocados aqui e chama `nb_display_hal_shell_flush_rect_and_swap()`
com a região suja do frame.

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
um frame (~61 ms @ 20 MHz; ~31 ms @ 40 MHz; ~20 ms @ 60 MHz).

**Staging PSRAM→SRAM:** a documentação oficial do ESP-IoT-Solution confirma
que, nesses targets, o driver SPI não faz DMA direto a partir da PSRAM. Se o
buffer de pixels está na PSRAM, o driver copia para SRAM interna antes de
transferir por DMA. Portanto o "bounce buffer" é o caminho suportado, não uma
falha a contornar. Em S4.1a, o `display_hal` passou a controlar esse staging:
aloca uma única janela interna DMA-capable (~19 KB), copia a região suja do
framebuffer PSRAM linha a linha e envia esse bloco compacto ao `esp_lcd`.
Isso evita alocações transitórias grandes do driver e mantém só uma
transferência em voo.

**Dirty rectangles:** o renderer retorna a união conservadora entre os olhos
do frame anterior e do frame atual. O primeiro frame continua full-screen; os
seguintes transmitem só a região suja, alinhada para múltiplos de 32 bytes e
fatiada para caber no staging fixo. O log de `face_demo` inclui `flush_kb`
para provar a redução de bytes no soak.

**Medição de S4.1a (2026-07-06):** com áudio ativo em paralelo, a
correção saiu de ~20,8 fps / `flush_ms` ~40 ms para
`fps=30.3 draw_ms=7.44 flush_ms=15.06 flush_kb=54.9`, mantendo
`audio_bringup` limpo (`ESP_OK`, `rx_ovf=0`, `tx_ovf=0`, `timeouts=0`).
Ou seja: o ganho veio de mandar menos bytes, não de subir clock SPI.

**Soak final de S4.1a (30 min):** `docs/bringup/s4_1a_display_audio_soak_20260706.log`
fechou com `fps_avg=30.3003`, `fps_min=30.3`, `flush_ms_avg=15.1033`,
`flush_kb_avg=55.0469`, `heap_min=170535`, `psram_min=16461580`,
`rx_ovf_max=0`, `tx_ovf_max=0`, `rx_timeout_max=0`, `tx_timeout_max=0`,
sem panic nem reboot durante a janela útil. Gate fechado.

**Coerência de cache PSRAM:** o caminho normal de S4.1a não chama mais
`esp_cache_msync()` por banda. A CPU lê o framebuffer em PSRAM e copia para
SRAM interna DMA-capable antes de enfileirar o DMA; o DMA não lê mais direto
do framebuffer PSRAM.

**Clock SPI:** em S2.1, 40 MHz foi o clock limpo conservador depois de
corrigir GND/orientação/cache. No reensaio de S4.1a (2026-07-06), 80 MHz
apresentou artefatos; 60 MHz ficou visualmente normal em teste manual, mas
não melhorou o fps observado (~20,8/20,9). O clock de produção voltou a
40 MHz; a correção de S4.1a reduz bytes transmitidos em vez de depender de
overclock de bancada.

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
