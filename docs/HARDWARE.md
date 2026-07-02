# NoiseBot 2 — Hardware

Placa única: **Freenove ESP32-S3-WROOM CAM N16R8** (16 MB flash, 8 MB PSRAM
octal). Todos os periféricos do v1, USB nativo restaurado.

> **Status:** os itens marcados `SPIKE S0` são plausíveis mas não provados —
> ver `S0_BRINGUP.md`. O restante foi validado em bancada no v1 (monólito
> branch `master` do repo original, `nb_hw_config.h`).

## 1. Mapa de pinos

| GPIO | Função | Dir. | Periférico | Status | Nota |
| --- | --- | --- | --- | --- | --- |
| 0 | BOOT | ← | strap | fixo | não usar |
| 1 | Speaker DIN (I2S) | → | `i2s_std` duplex | validado v1 | sacrifica T1 |
| 2 | Touch corporal | ← | `touch_sensor` TOUCH2 | validado v1 | fita de cobre |
| 3 | **Servo SCS bus 1 fio** | ↔ | UART1 half-duplex via GPIO matrix | **SPIKE S0.1** | open-drain + pull-up 4.7k; strap JTAG-sel tolerante |
| 4 | Câmera SIOD / I2C SDA | ↔ | `i2c_master` + SCCB | validado v1 | **barramento de expansão** (IMU, MPR121, fuel gauge, touchscreen) |
| 5 | Câmera SIOC / I2C SCL | ↔ | idem | validado v1 | pull-up externo |
| 6 | Câmera VSYNC | ← | DVP | fixo na placa | nunca realocar |
| 7 | Câmera HREF | ← | DVP | fixo | |
| 8–13 | Câmera D2,D1,D3,D0,D4,PCLK | ← | DVP | fixo | |
| 14 | Mic SD (I2S) | ← | `i2s_std` duplex | validado v1 | sacrifica TOUCH14 |
| 15 | Câmera XCLK | → | DVP | fixo | |
| 16–18 | Câmera D7,D6,D5 | ← | DVP | fixo | |
| 19/20 | **USB nativo D-/D+** | ↔ | `usb_serial_jtag` + CDC | restaurado | era o preço do servo no v1 |
| 21 | Display MOSI | → | SPI2 | validado v1 | |
| 26–37 | flash/PSRAM | — | — | fixo | 35–37 no header mas NC |
| 38 | SD CMD | ↔ | `sdmmc` 1-bit | validado v1 | onboard |
| 39 | SD CLK | → | `sdmmc` | validado v1 | |
| 40 | SD DATA0 | ↔ | `sdmmc` | validado v1 | |
| 41 | Áudio BCLK | → | `i2s_std` duplex | validado v1 | compartilhado mic+spk |
| 42 | Áudio WS/LRCK | → | idem | validado v1 | compartilhado |
| 43/44 | Console UART0 (CP2102) | ↔ | fixo na placa | fixo | trilhas soldadas — **nunca repurposar** (lição v1) |
| 45 | Display DC | → | SPI2 | validado v1 | strap VDD_SPI, funcional |
| 46 | **WS2812 externos (dado)** | → | `rmt_tx` | **SPIKE S0.2** | dado idle-LOW compatível com strap PD; level via pixel sacrificial |
| 47 | Display SCLK | → | SPI2 | validado v1 | 50 MHz (60/80 falharam) |
| 48 | LED status onboard (azul) | → | `gpio` | validado v1 | independente dos WS2812 externos |

Display ST7789: CS→GND, RST via SWRESET (software), sem MISO, sem BL —
**3 GPIOs no total** (21/45/47), configuração já validada no v1 a 50 MHz.

**Zero GPIO livre por design.** Expansão futura entra pelo I2C (4/5):
MPR121 (touch 3 zonas), MPU-6050 (INT por polling ou expander), MAX17048,
touchscreen capacitivo. Custo de pino: zero.

## 2. Lições elétricas herdadas (obrigatórias)

1. **DOUT de WS2812 onboard não é exposto** em placas dev → daisy-chain com o
   LED da placa é impossível; paralelo escraviza o primeiro pixel externo.
   Por isso os externos têm pino próprio (46) e o onboard (48) fica como
   status independente.
2. **WS2812 a 5 V com dado 3.3 V é marginal** (VIH ≈ 3,5 V). Solução: primeiro
   pixel alimentado a ~4,3 V (diodo em série no 5 V) regenera o sinal para os
   demais, ou level shifter dedicado.
3. **GPIO46 nunca com sinal idle-HIGH** (corrompe download mode). Dado WS2812 é
   idle-LOW; DIN é alta impedância — compatível. Validar no S0.2.
4. **43/44 pertencem ao CP2102** (soldado). Não é pino disponível.
5. **Gate elétrico antes de servo** (S6.1): o v1 perdeu uma Waveshare por
   curto 5V↔3V3. Checklist assinado (proteção reversa, fuse por trilho,
   isolação da trilha de servo, brownout sob stall) é pré-condição bloqueante
   de `motion_safety` verde.
6. Barramento SCS0009 é half-duplex TTL nativo; o FE-TTLinker é dispensável se
   o S0.1 validar o 1 fio direto (open-drain + pull-up).

## 3. Budgets de memória (enforcement em `QUALITY.md`)

| Recurso | Budget | Observação |
| --- | --- | --- |
| PSRAM 8 MB | sprites face 2×320×240×2 ≈ 300 KB; framebuffer câmera ≤ 200 KB; ring TTS 96 KB; caches ≤ 1 MB; **≥ 4 MB livres em runtime** | medido em smoke de bancada por release |
| SRAM | zero framebuffer; DMA + stacks + filas; teto estático do `.map` no CI | número definido em S1.1 e só sobe com medição |
| Flash 16 MB | OTA A/B 2×3 MB + NVS + coredump 64 KB + assets | `partitions.csv` versionado |

## 4. Energia

Herdado do v1: 5 V desktop (fonte RPi4 3A), brownout callback → torque-off
imediato. Orçamento detalhado será portado/medido em S6 (servos são o item
dominante). Bateria/LiPo: adiado, entra por I2C + trilha própria em revisão
futura de hardware.

## 5. Periféricos

| Periférico | Interface | Fase |
| --- | --- | --- |
| ST7789 2" 320×240 | SPI2 (3 pinos) | S2 |
| WS2812 × 2 externos | RMT GPIO46 | S3 |
| Touch cobre | TOUCH2 | S3 |
| INMP441 + MAX98357A | I2S0 full-duplex 16 kHz | S4 |
| OV2640 | DVP fixo + SCCB | S5 |
| microSD | SDMMC 1-bit | S1 (logs) / S5 (assets) |
| SCS0009 × 2 | UART 1 fio GPIO3 | S6 |
| IMU / bateria / MPR121 / touchscreen | I2C 4/5 | pós-S7 |
