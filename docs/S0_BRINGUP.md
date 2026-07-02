# S0 — Spikes de Bancada (bring-up, rota Waveshare)

Os experimentos que destravam o projeto. **Nenhuma fase posterior começa sem
o S0 fechado**, porque o pinout congela aqui. Material: Waveshare N32R16V,
módulo microSD, ArduCam Mega, ST7789, osciloscópio/analisador lógico,
protoboard, fonte de bancada com limite de corrente.

> Segurança de bancada (lição v1 — uma N32R16V morreu por curto 5V↔3V3):
> conferir com multímetro **antes de energizar** cada montagem; limite de
> corrente em 500 mA em todos os spikes (não há servo no S0).
> Lembrete N32R16V: **GPIO47/48 são 1.8V — nunca conectar lógica 3.3V.**

O spike do servo 1-fio da rota Freenove **foi eliminado**: nesta rota o servo
usa UART real (17/18) via FE-TTLinker — bring-up de servo é trabalho normal
da S6, atrás do gate elétrico S6.1.

---

## S0.1 — Health check da placa + display no SPI2

**Hipóteses:** a unidade N32R16V atual está eletricamente sã; o ST7789 roda a
50 MHz nos pinos IO-MUX (SCLK 12, MOSI 11, CS 10, DC 14, RST via SWRESET).

**Procedimento:**

1. Health check: registrar MAC (`esptool chip_id`), medir 3V3 e 5V em
   repouso e sob carga de display, boot limpo 10×. Registrar em
   `docs/bringup/` (é a unidade substituta — a que morreu está documentada
   no v1).
2. Display: padrão animado 30 fps por 1 h, double buffer em PSRAM, DMA.
3. Testar 50 MHz; se instável, degrau para 40 MHz e registrar.

**Gate de saída:**
- [ ] MAC e medições elétricas registradas
- [ ] 1 h a 30 fps sem artefato, frequência final registrada
- [ ] PSRAM octal reconhecida (16 MB) e sprites alocados nela

---

## S0.2 — Câmera SPI no barramento compartilhado — **ADIADO**

**Decisão 2026-07-02:** câmera fora do escopo v2.0 (form factor StackChan sem
cavidade interna para o módulo). Este spike fica registrado e **executa
se/quando a fase S5 voltar**, sem mudança de pinout: CS 9 e MISO 13 estão
reservados.

Roteiro preservado para o retorno: bring-up isolado (JPEG íntegro 100/100,
decodificado no PC) → compartilhamento com render 30 fps (bus-lock ESP-IDF)
→ medir tempo de captura/fps mínimo/erros SPI. Gate: fps ≥ 28 durante
capturas, zero erro de barramento em 30 min, baseline p95 < 400 ms.

---

## S0.3 — microSD externo + contenção total

**Hipóteses:** SDMMC 1-bit via GPIO matrix (CLK 15, CMD 16, D0 6) funciona
estável; o conjunto **render + I2S duplex + escrita SD** roda sem underrun de
áudio (o teste que nenhuma geração fez completo).

**Procedimento:**

1. Bring-up SD isolado: mount FATFS, escrita/leitura 10 MB, remount após
   remoção a quente.
2. Carga combinada 30 min: render 30 fps + tom contínuo/captura mic (I2S
   duplex 16 kHz) + append de log no SD a cada 1 s.
3. Registrar: underruns, fps mínimo, latência de escrita SD, heap/PSRAM,
   temperatura.

**Gate de saída:**
- [ ] Zero underrun de áudio em 30 min
- [ ] fps ≥ 28 sustentado; escrita SD nunca bloqueia áudio/render
- [ ] PSRAM livre ≥ 10 MB durante o teste inteiro
- [ ] Números registrados (viram baseline de `QUALITY.md` §4)

---

## S0.4 — Congelamento do pinout

Com S0.1–S0.3 fechados (ou fallbacks decididos): atualizar `HARDWARE.md`
removendo os marcadores `SPIKE`, registrar evidências (fotos do osciloscópio,
logs, JPEGs de teste) em `docs/bringup/`, e taggear `pinout-v1.0`.

**Gate de saída:**
- [ ] `HARDWARE.md` sem pendências de spike
- [ ] Decisões e medições registradas em `docs/bringup/`
- [ ] Tag criada — a partir daqui, mudança de pino exige RFC no ROADMAP
