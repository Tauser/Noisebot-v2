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

## 2. GPIOs por módulo

Resumo para montagem: use esta seção primeiro. A lista de pinos proibidos e
reservados fica no fim da seção.

### Display ST7789 2" (S0.1/S2.1)

| Sinal | GPIO | Periférico | Status | Nota |
| --- | --- | --- | --- | --- |
| CS | 10 | SPI2 `FSPICS0` | **SPIKE S0.1** | obrigatório; barramento compartilhado |
| MOSI | 11 | SPI2 `FSPID` | **SPIKE S0.1** | dados para display e câmera futura |
| SCLK | 12 | SPI2 `FSPICLK` | **SPIKE S0.1** | IO MUX; alvo 50 MHz após validar |
| DC | 14 | `gpio` | **SPIKE S0.1** | comando/dado |
| RST | — | — | fixo | sem pino; reset via SWRESET |
| BL | — | — | fixo | sem controle no firmware |
| MISO | — | — | não usado | display não lê dados |

### Câmera SPI futura (adiada)

| Sinal | GPIO | Periférico | Status | Nota |
| --- | --- | --- | --- | --- |
| CS | 9 | SPI2 | reservado | câmera adiada; não reaproveitar |
| MISO | 13 | SPI2 `FSPIQ` | reservado | retorno da câmera futura |
| MOSI | 11 | SPI2 | compartilhado | mesmo barramento do display |
| SCLK | 12 | SPI2 | compartilhado | mesmo barramento do display |

### microSD externo (S0.3/S1.3)

| Sinal | GPIO | Periférico | Status | Nota |
| --- | --- | --- | --- | --- |
| DATA0 | 6 | `sdmmc` 1-bit | **SPIKE S0.3** | módulo externo |
| CLK | 15 | `sdmmc` | **SPIKE S0.3** | |
| CMD | 16 | `sdmmc` | **SPIKE S0.3** | |

### Áudio I2S (S4)

| Sinal | GPIO | Periférico | Status | Nota |
| --- | --- | --- | --- | --- |
| Mic SD | 39 | `i2s_std` duplex | validado v1 | INMP441 |
| BCLK | 40 | `i2s_std` duplex | validado v1 | mic + speaker |
| WS/LRCK | 41 | `i2s_std` duplex | validado v1 | mic + speaker |
| Speaker DIN | 42 | `i2s_std` duplex | validado v1 | MAX98357A |

### Servos e energia de motion (S6)

| Sinal | GPIO | Periférico | Status | Nota |
| --- | --- | --- | --- | --- |
| Servo rail enable | 3 | `gpio` | perfil B | MOSFET/load switch; pull-down externo obrigatório |
| Servo PAN | 17 | perfil B: `ledc` PWM 50 Hz; perfil A: UART TX | decisão 2026-07-02 | mesmo pino nos dois perfis |
| Servo TILT | 18 | perfil B: `ledc` PWM 50 Hz; perfil A: UART RX | decisão 2026-07-02 | cabeamento é gate S6.1/S6.2 |
| Corrente do trilho | I2C 4/5 | INA219 | perfil B | proxy de stall por corrente |

### LEDs (S3)

| Sinal | GPIO | Periférico | Status | Nota |
| --- | --- | --- | --- | --- |
| WS2812 externos | 21 | `rmt_tx` | validado v1 | usar level shift ou pixel sacrificial |
| LED status onboard | 38 | `rmt_tx` | fixo na placa | WS2812 da própria placa |

### Toque, IMU e expansão I2C

| Sinal | GPIO | Periférico | Status | Nota |
| --- | --- | --- | --- | --- |
| Touch corporal | 2 | `touch_sensor` TOUCH2 | validado v1 | fita de cobre |
| I2C SDA | 4 | `i2c_master` I2C0 | validado v1 | INA219, touch ctrl, IMU, fuel gauge |
| I2C SCL | 5 | `i2c_master` I2C0 | validado v1 | pull-up 4.7k |
| Monitor 5V | 7 | `adc_oneshot` ADC1_CH6 | validado v1 | divisor 68k/56k |
| Touchscreen INT | 1 | `gpio` ISR | reservado | pós-v2.0 |
| IMU INT | 8 | `gpio` ISR | reservado | MPU-6050 pós-v2.0 |

### USB, console e straps

| Sinal | GPIO | Periférico | Status | Nota |
| --- | --- | --- | --- | --- |
| BOOT | 0 | strap | fixo | não usar |
| USB nativo D-/D+ | 19/20 | `usb_serial_jtag`/CDC | fixo | console/JTAG USB |
| Console UART0 | 43/44 | CH343 | fixo | programming/console |
| Straps | 45/46 | strap | evitar | 46 nunca com sinal idle-HIGH |

### Proibidos/indisponíveis

| GPIO | Motivo | Regra |
| --- | --- | --- |
| 33/34 | não expostos no header | indisponíveis |
| 35/36/37 | NC por causa da PSRAM octal | indisponíveis |
| 47/48 | domínio VDD_SPI 1.8V | **nunca ligar lógica 3.3V** |

Livres após alocação completa: nenhum dedicado. GPIO1/8 seguem reservados
para touch/IMU futuros; expansão adicional deve entrar pelo I2C (4/5).

### Perfis de motion (decisão 2026-07-02)

| | Perfil B — provisório (inicial) | Perfil A — upgrade |
| --- | --- | --- |
| Servos | 2× MG90S (PWM 50 Hz, LEDC em 17/18) | 2× SCS0009 via FE-TTLinker (UART em 17/18) |
| Feedback | nenhum (servo cego) | posição/carga/temp/tensão pelo barramento |
| Stall | **proxy**: INA219 no trilho B (I2C) + corte por MOSFET (GPIO3) | leitura de carga do servo |
| Temperatura | não medível → limite de duty (nunca segurar contra carga) | leitura direta |
| Repouso | **detach** (PWM parado — silêncio, sem zumbido) | torque idle configurável |
| Custo | ~R$ 20–30 (par) + INA219 ~R$ 12 + MOSFET | ~R$ 150+ (par) + TTLinker |
| Precedente | Stack-chan clássico usa SG90/MG90S | v1 (protocolo SCS já implementado) |

O `servo_hal` é **interface dupla desde o S6.2**: `servo_hal_pwm` e
`servo_hal_scs` implementam o mesmo contrato; `motion_safety` seleciona o
perfil por capability (`has_feedback`). Upgrade B→A: trocar casca +
re-executar gates S6.2/S6.5 em bancada. Sem refactor.

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
| 2× MG90S + INA219 + MOSFET (load switch) | LEDC 17/18 + I2C + GPIO3 | S6 (perfil B) | motion provisório; ver §Perfis de motion |
| 2× SCS0009 + FE-TTLinker + power board | UART 17/18 | upgrade perfil A | quando o produto justificar feedback real |
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
| Servos (perfil B: MG90S PWM → perfil A: SCS0009 UART) | 17/18 + INA219 (I2C) + enable GPIO3 | S6 |
| Câmera SPI (adiada) | SPI2 (CS 9, MISO 13) reservados | pós-v2.0 |
| Tela touch / IMU / bateria | I2C (+INT 1/8) | pós-v2.0 |
