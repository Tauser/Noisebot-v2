# S0 — Spikes de Bancada (bring-up)

Os três experimentos que destravam o projeto. **Nenhuma fase posterior começa
sem o S0 fechado**, porque o pinout congela aqui. Material: Freenove N16R8
sobressalente (não a que roda o v1, se possível), osciloscópio/analisador
lógico, protoboard, fonte 5 V de bancada com limite de corrente.

> Segurança de bancada (lição v1 — Waveshare morta por curto 5V↔3V3):
> conferir com multímetro **antes de energizar** cada montagem; limite de
> corrente da fonte em 500 mA nos spikes sem servo, 1,5 A com servo.

---

## S0.1 — Servo SCS 1 fio no GPIO3

**Hipótese:** o barramento SCS0009 (half-duplex TTL) funciona com UART1 TX e
RX roteados para o **mesmo** GPIO3 via GPIO matrix, open-drain com pull-up,
dispensando o FE-TTLinker e os pinos 19/20.

**Montagem:**

1. GPIO3 → linha de dados do servo; pull-up 4,7 kΩ a 3,3 V.
2. TX em open-drain (`GPIO_MODE_INPUT_OUTPUT_OD`); RX ligado ao mesmo pino
   pela matrix (`esp_rom_gpio_connect_in_signal`).
3. Alimentação do servo: 5 V da fonte de bancada, GND comum, **nunca** do
   regulador da placa.
4. Echo do próprio TX chega no RX: o driver descarta os N bytes ecoados antes
   de ler a resposta (técnica padrão de barramento 1 fio).

**Procedimento:** PING (1 Mbps; fallback 500 kbps) → leitura de posição →
leitura contínua 20 Hz por 10 min → escrita de posição com torque **somente
com o servo solto na bancada, sem carga mecânica**.

**Gate de saída:**
- [ ] PING responde de forma estável (≥ 999/1000)
- [ ] Leitura de posição/temp/load 20 Hz por 10 min sem erro de frame
- [ ] Osciloscópio: bordas limpas, sem contenção na transição TX→RX
- [ ] Boot normal da placa com o servo conectado (strap do GPIO3 não afetado)
- [ ] Registrar: baud final, tempo de turnaround, config exata da matrix

**Fallback documentado:** servo volta ao esquema v1 (UART em 19/20 via
TTLinker, perde USB nativo). O blueprint sobrevive; só o mapa de pinos muda.

---

## S0.2 — WS2812 externos no GPIO46

**Hipótese:** GPIO46 (strap ROM msg, pull-down fraco) dirige os 2 WS2812
externos com RMT; dado idle-LOW é compatível com o strap; DIN em alta
impedância não interfere no download mode.

**Montagem:**

1. GPIO46 → DIN do pixel 1. Pixel 1 alimentado a ~4,3 V (diodo 1N4007 em
   série no 5 V) — ele regenera o sinal para o pixel 2 a 5 V pleno
   (*pixel sacrificial* como level shifter).
2. Capacitor 100 µF no rail dos LEDs; resistor 300 Ω em série no dado.

**Procedimento:** animação de teste (fade + chase) 30 min; ciclo de 20 reboots;
20 uploads consecutivos via esptool com os LEDs conectados; boot com LEDs
energizados antes da placa.

**Gate de saída:**
- [ ] Cores estáveis, zero flicker/pixel fantasma em 30 min
- [ ] 20/20 reboots normais (strap não violado)
- [ ] 20/20 uploads esptool ok com LEDs conectados
- [ ] LED onboard (48) permanece independente

**Fallback:** dado dos LEDs em GPIO3 e servo nos 19/20 (esquema v1 completo).

---

## S0.3 — Contenção câmera + display + áudio (o teste que o v1 nunca fez)

**Hipótese:** um único S3 sustenta render 30 fps + I2S duplex + captura JPEG
sob demanda sem underrun de áudio nem queda visível de fps (PSRAM octal e DMA
dão conta; o CoreS3 comercial sugere que sim — medir, não crer).

**Montagem:** firmware de spike (fora da árvore de produto) com: render de
padrão animado 30 fps em double buffer PSRAM via SPI 50 MHz; tom contínuo no
speaker + captura de mic em loopback (I2S duplex 16 kHz); captura JPEG QVGA a
cada 2 s com `esp_camera` (driver validado no v1/DM4).

**Procedimento:** 30 min contínuos registrando: fps médio/mínimo, underruns
I2S, tempo de captura JPEG, heap/PSRAM livres, temperatura.

**Gate de saída:**
- [ ] Zero underrun de áudio em 30 min
- [ ] fps ≥ 28 sustentado, inclusive durante capturas
- [ ] Captura JPEG < 300 ms p95
- [ ] PSRAM livre ≥ 4 MB durante o teste inteiro
- [ ] Números registrados neste doc (viram o baseline de `QUALITY.md` §4)

**Fallback:** rebaixar fps para 20 durante captura (câmera é sob demanda e
rara) — decisão registrada com a medição.

---

## S0.4 — Congelamento do pinout

Com S0.1–S0.3 fechados (ou fallbacks decididos): atualizar `HARDWARE.md`
removendo os marcadores `SPIKE`, registrar evidências (fotos de tela do
osciloscópio, logs) em `docs/bringup/`, e taggear `pinout-v1.0`.

**Gate de saída:**
- [ ] `HARDWARE.md` sem pendências de spike
- [ ] Decisões e medições registradas
- [ ] Tag criada — a partir daqui, mudança de pino exige RFC no ROADMAP
