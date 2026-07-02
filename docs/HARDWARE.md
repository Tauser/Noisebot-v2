# NoiseBot 2 — Hardware

Placa única: **Waveshare ESP32-S3-DEV-KIT N32R16V** (módulo WROOM-2: 32 MB
flash octal OPI + 16 MB PSRAM octal). Decisão registrada em 2026-07-01:
a Waveshare oferece ~11 GPIOs livres (vs. zero na Freenove), UART real para
os servos, pinos de INT para touchscreen/IMU e 2× memória — ao custo de SD
externo e câmera SPI (compra).

> **Rota alternativa preservada:** o desenho equivalente na Freenove N16R8
> (câmera e SD onboard grátis, ao custo de zero GPIO livre e dois spikes de
> risco) está em `HARDWARE_FREENOVE.md`, com o procedimento de troca. Este
> documento é o ativo.

> **Status:** itens `SPIKE S0.x` são plausíveis mas não provados — ver
> `S0_BRINGUP.md`. Itens `validado v1` vêm do mapa DMM da Waveshare e do
> monólito Freenove do NoiseBot v1.

> **Atenção — histórico elétrico:** uma unidade N32R16V do v1 morreu por
> curto 5V↔3V3 (2026-06-20). A unidade atual deve ter o health check do
> S0.1 registrado (MAC, teste de reguladores) antes de qualquer montagem.

## 1. Armadilhas específicas da placa (confirmadas no esquemático v1)

1. **GPIO47/48 = domínio VDD_SPI 1.8V** no N32R16V — **nunca** usar para
   lógica 3.3V.
2. **GPIO35–37 expostos no header P2 porém NC** (PSRAM octal). Inoperantes.
3. **GPIO33/34 não são expostos** no header. Indisponíveis.
4. **GPIO38 = WS2812 onboard** — vira LED de status; não é GPIO limpo.
5. **GPIO0** = BOOT; **GPIO45/46** = straps (46: nunca sinal idle-HIGH).
6. **GPIO43/44** = console CH343; **GPIO19/20** = USB nativo via hub.

## 2. Mapa de pinos

| GPIO | Função | Dir. | Periférico | Status | Nota |
| --- | --- | --- | --- | --- | --- |
| 0 | BOOT | ← | strap | fixo | não usar |
| 1 | Touchscreen INT | ← | `gpio` ISR | reservado | entra com a tela touch (pós-v2.0) |
| 2 | Touch corporal | ← | `touch_sensor` TOUCH2 | validado v1 | fita de cobre |
| 3 | Reserva | — | — | livre | strap JTAG-sel; evitar p/ crítico |
| 4 | I2C SDA | ↔ | `i2c_master` I2C0 | validado v1 | **barramento de expansão**: touch ctrl, IMU, fuel gauge, charger |
| 5 | I2C SCL | ↔ | idem | validado v1 | pull-up 4.7k |
| 6 | SD DATA0 | ↔ | `sdmmc` 1-bit (matrix) | **SPIKE S0.3** | módulo microSD externo |
| 7 | Monitor 5V | A | `adc_oneshot` ADC1_CH6 | validado v1 | divisor 68k/56k |
| 8 | IMU INT | ← | `gpio` ISR | reservado | MPU-6050 (pós-v2.0) |
| 9 | Câmera CS | → | `spi_master` SPI2 | **reservado (câmera adiada)** | slot para retorno da visão pós-v2.0 |
| 10 | Display CS | → | `spi_master` SPI2 (FSPICS0) | **SPIKE S0.1** | IO MUX nativo |
| 11 | SPI MOSI | → | SPI2 (FSPID) | **SPIKE S0.1** | compartilhado display+câmera futura |
| 12 | SPI SCLK | → | SPI2 (FSPICLK) | **SPIKE S0.1** | IO MUX → 50 MHz+ |
| 13 | SPI MISO | ← | SPI2 (FSPIQ) | **reservado (câmera adiada)** | display não usa MISO |
| 14 | Display DC | → | `gpio` | **SPIKE S0.1** | |
| 15 | SD CLK | → | `sdmmc` | **SPIKE S0.3** | |
| 16 | SD CMD | ↔ | `sdmmc` | **SPIKE S0.3** | |
| 17 | Servo UART1 TX | → | `uart` 1 Mbps | validado v1 (mapa) | **FE-TTLinker — UART real, sem truque de 1 fio** |
| 18 | Servo UART1 RX | ← | idem | validado v1 (mapa) | cabeamento é gate S6.1/S6.2 |
| 19/20 | USB nativo D-/D+ | ↔ | `usb_serial_jtag`/CDC | fixo | livre para console/JTAG |
| 21 | WS2812 externos | → | `rmt_tx` | validado v1 | level-shift p/ 5V (pixel sacrificial ou shifter) |
| 33–37 | — | — | — | indisponíveis | ver §1 |
| 38 | LED status onboard | → | `rmt_tx` | fixo na placa | WS2812 da placa |
| 39 | Mic SD (I2S) | ← | `i2s_std` duplex | validado v1 | perde JTAG por pinos (usar USB-JTAG) |
| 40 | Áudio BCLK | → | idem | validado v1 | compartilhado mic+spk |
| 41 | Áudio WS/LRCK | → | idem | validado v1 | compartilhado |
| 42 | Speaker DIN | → | idem | validado v1 | MAX98357A |
| 43/44 | Console UART0 | ↔ | CH343 | fixo | programming/console |
| 45/46 | — | — | straps | evitar | |
| 47/48 | — | — | **1.8V** | **nunca** | domínio VDD_SPI do N32R16V |

Display ST7789: RST via SWRESET, sem BL, sem MISO (config validada no v1 a
50 MHz) — mas aqui **CS é obrigatório** (GPIO10), porque o barramento SPI2 é
compartilhado com a câmera.

Livres após alocação completa: **GPIO3** (+ 1/8 até a tela touch/IMU
entrarem). Expansão adicional: I2C (4/5).

## 3. Lições elétricas herdadas (obrigatórias)

1. **Gate elétrico antes de servo** (S6.1): o v1 perdeu uma N32R16V por curto
   5V↔3V3. Checklist assinado (proteção reversa, fuse por trilho, isolação da
   trilha de servo, GND comum, brownout sob stall) é pré-condição bloqueante.
2. WS2812 a 5 V com dado 3.3 V é marginal (VIH ≈ 3,5 V): pixel sacrificial a
   ~4,3 V ou level shifter. LED onboard (38) permanece independente.
3. Fonte de bancada com limite de corrente em todos os spikes; multímetro
   antes de energizar qualquer montagem nova.
4. Trilha de energia dos servos (placa de power do TTLinker) **nunca** passa
   pelo 5 V da placa dev — alimentação própria com GND comum.

## 4. Compras/novos módulos desta rota

| Item | Interface | Fase | Nota |
| --- | --- | --- | --- |
| Módulo microSD (SPI/SDMMC) | SDMMC 1-bit (6/15/16) | S0.3 | barato; substitui o SD onboard da Freenove |
| ~~Câmera~~ **ADIADA** (2026-07-02) | SPI (CS 9 reservado) | pós-v2.0 | form factor StackChan sem cavidade p/ módulo 33×33×17 mm; opções de retorno: ArduCam Mega **M12** (foco ajustável p/ distância de mesa; só a lente passa pelo furo, corpo precisa de espaço interno) ou câmera WiFi independente → server |
| Tela touch (ctrl I2C) | I2C + INT (1) | pós-v2.0 | reserva de pinos já feita |
| MPU-6050 | I2C + INT (8) | pós-v2.0 | |
| Placas de energia (charger/boost/fuel gauge) | I2C + status | pós-v2.0 | ver §3.4 |
| Power board do TTLinker | trilho 5V próprio | S6.1 | GND comum, nunca 5V da dev board |

A OV2640/microSD onboard da **Freenove** ficam com ela — a placa vira spare/
referência do v1 (continua rodando o v1 até S7.6).

## 5. Budgets de memória (enforcement em `QUALITY.md`)

| Recurso | Budget | Observação |
| --- | --- | --- |
| PSRAM 16 MB | sprites face ≈ 300 KB; ring TTS 96 KB; caches ≤ 2 MB; **≥ 10 MB livres** | folga para touchscreen UI e câmera futuras |
| SRAM | zero framebuffer; DMA + stacks + filas; teto do `.map` no CI | definido em S1.1 |
| Flash 32 MB | OTA A/B 2×4 MB + NVS + coredump + assets ~23 MB | `partitions.csv` versionado |

## 6. Energia

5 V desktop (fonte RPi4 3A) na fase atual; brownout → torque-off imediato.
Servos com trilho próprio via power board do TTLinker (GND comum). Bateria
(charger + boost + fuel gauge) entra pós-v2.0 pelo I2C, com revisão do
orçamento de energia na entrada.

## 7. Periféricos por fase

| Periférico | Interface | Fase |
| --- | --- | --- |
| ST7789 2" 320×240 | SPI2 (10/11/12/14) | S0.1 → S2 |
| WS2812 × 2 externos | RMT GPIO21 | S3 |
| Touch cobre | TOUCH2 | S3 |
| INMP441 + MAX98357A | I2S duplex (39–42) | S4 |
| microSD externo | SDMMC 1-bit (6/15/16) | S0.3 → S1 (logs) |
| SCS0009 × 2 + TTLinker | UART1 (17/18) | S6 |
| Câmera SPI (adiada) | SPI2 (CS 9, MISO 13) reservados | pós-v2.0 |
| Tela touch / IMU / bateria | I2C (+INT 1/8) | pós-v2.0 |
