# NoiseBot 2 — Hardware: Rota Alternativa Freenove (NÃO ATIVA)

> **Status: alternativa documentada, não ativa.** A rota ativa é a Waveshare
> N32R16V (`HARDWARE.md`, decisão 2026-07-01). Este documento preserva o
> desenho single-MCU na **Freenove ESP32-S3-WROOM CAM N16R8** (16 MB flash /
> 8 MB PSRAM) para o caso de reversão. Se ativada: seguir o procedimento de
> troca da §6.

**Quando esta rota venceria:** se a câmera voltar a ser prioridade (OV2640
onboard grátis + SD onboard grátis), ou se a unidade Waveshare falhar e o
custo de reposição importar.

**O preço desta rota:** zero GPIO livre, dois spikes de risco (servo 1 fio e
WS2812 no strap), touch/IMU futuros sem pino de INT (polling ou expansor
I2C), metade da memória, e a placa atual roda o v1 (trocar = aposentar o v1
antes de S7 ou comprar outra Freenove).

## 1. Mapa de pinos

| GPIO | Função | Dir. | Periférico | Status | Nota |
| --- | --- | --- | --- | --- | --- |
| 0 | BOOT | ← | strap | fixo | não usar |
| 1 | Speaker DIN (I2S) | → | `i2s_std` duplex | validado v1 | sacrifica T1 |
| 2 | Touch corporal | ← | `touch_sensor` TOUCH2 | validado v1 | fita de cobre |
| 3 | **Servo SCS bus 1 fio** | ↔ | UART1 half-duplex via GPIO matrix | **SPIKE F-A** | open-drain + pull-up 4.7k; strap JTAG-sel tolerante |
| 4 | Câmera SIOD / I2C SDA | ↔ | `i2c_master` + SCCB | fixo na placa | barramento de expansão (sem INT disponível) |
| 5 | Câmera SIOC / I2C SCL | ↔ | idem | fixo | pull-up externo |
| 6–13 | Câmera VSYNC/HREF/D2/D1/D3/D0/D4/PCLK | ← | DVP | fixo | **onboard — nunca realocar** |
| 14 | Mic SD (I2S) | ← | `i2s_std` duplex | validado v1 | sacrifica TOUCH14 |
| 15–18 | Câmera XCLK/D7/D6/D5 | — | DVP | fixo | |
| 19/20 | USB nativo | ↔ | `usb_serial_jtag`/CDC | condicionado | **livre só se SPIKE F-A passar**; fallback: servo aqui (perde USB, solução v1) |
| 21 | Display MOSI | → | SPI2 | validado v1 | |
| 38/39/40 | SD CMD/CLK/DATA0 | ↔ | `sdmmc` 1-bit | fixo onboard | grátis nesta rota |
| 41 | Áudio BCLK | → | I2S duplex | validado v1 | compartilhado |
| 42 | Áudio WS/LRCK | → | idem | validado v1 | compartilhado |
| 43/44 | Console UART0 (CP2102) | ↔ | fixo na placa | fixo | trilhas soldadas — nunca repurposar |
| 45 | Display DC | → | SPI2 | validado v1 | strap VDD_SPI, funcional |
| 46 | **WS2812 externos** | → | `rmt_tx` | **SPIKE F-B** | dado idle-LOW compatível com strap PD; pixel sacrificial ~4,3 V |
| 47 | Display SCLK | → | SPI2 | validado v1 | 50 MHz (60/80 falharam) |
| 48 | LED status onboard (azul) | → | `gpio` | validado v1 | DOUT de WS2812 onboard não existe — externos precisam de pino próprio |

Display ST7789: CS→GND, RST via SWRESET, sem MISO, sem BL — **3 GPIOs**
(21/45/47), validado a 50 MHz no v1. **Zero GPIO livre** após alocação.

## 2. Spikes exclusivos desta rota (substituem S0.1/S0.2 da rota ativa)

- **F-A — Servo 1 fio no GPIO3:** UART TX+RX no mesmo pino via GPIO matrix,
  open-drain + pull-up; echo do TX descartado pelo driver. Gate: PING
  ≥ 999/1000 a 1 Mbps (fallback 500 kbps), leitura 20 Hz/10 min sem erro,
  bordas limpas no osciloscópio, boot normal com servo conectado.
  **Fallback:** servo em 19/20 via TTLinker (validado no v1) — perde USB
  nativo, console fica só no CP2102.
- **F-B — WS2812 no GPIO46:** pixel sacrificial a ~4,3 V como level shift.
  Gate: 30 min sem flicker, 20/20 reboots, 20/20 uploads esptool com LEDs
  conectados. **Fallback:** dado dos LEDs no GPIO3 e servo nos 19/20
  (esquema v1 completo).
- **S0.3 (contenção)** desta rota inclui a câmera DVP: render + I2S duplex +
  captura `esp_camera` + SD — o teste que o v1 nunca fez completo.

## 3. Câmera e mecânica

- OV2640 onboard grátis via `esp_camera` (lição DM4.8 do v1: nunca
  `esp_video` para DVP+S3).
- O sensor fica soldado **na placa** — se a placa mora no corpo, a câmera
  não vê nada. Solução: cabo FPC de extensão 24-pin (7–15 cm), sensor no
  furo da cabeça. Integridade de sinal DVP em cabo longo é risco: validar
  no spike de contenção.
- Form factor StackChan: mesmo problema de cavidade da rota ativa se a
  placa inteira for para a cabeça (a Freenove tem ~65×28 mm).

## 4. Budgets (menores que a rota ativa)

| Recurso | Budget |
| --- | --- |
| PSRAM 8 MB | sprites ~300 KB + framebuffer câmera ≤ 200 KB + ring TTS 96 KB; **≥ 4 MB livres** |
| Flash 16 MB | OTA A/B 2×3 MB + coredump + assets ~9,8 MB |
| Expansão futura | só I2C **sem INT** (touch/IMU por polling ou expansor MCP23017/PCF8574) |

## 5. Comparação com a rota ativa (resumo da decisão)

| Critério | Waveshare (ativa) | Freenove (esta) |
| --- | --- | --- |
| GPIOs livres pós-alocação | 1–3 + INTs reservados | **zero** |
| Servo | UART real 17/18 (sem spike) | 1 fio GPIO3 (spike) ou perde USB |
| Câmera | SPI a comprar (adiada) | OV2640 grátis onboard |
| SD | módulo externo | grátis onboard |
| Touch/IMU futuros | INT dedicado (GPIO1/8) | polling / expansor |
| Memória | 32 MB / 16 MB | 16 MB / 8 MB |
| Armadilha elétrica | GPIO47/48 em 1.8V | straps 45/46 no caminho do display/LED |
| Custo incremental | SD + câmera (se voltar) | zero (se aposentar o v1) |

## 6. Procedimento de troca (se esta rota for ativada)

1. Registrar a decisão e o motivo no painel do `ROADMAP.md`.
2. Trocar `HARDWARE.md` ativo por este (e este vira o arquivado).
3. `sdkconfig.defaults`: flash 16 MB (`CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y`,
   remover `OCT_FLASH`); `partitions.csv`: OTA 2×3 MB, assets 0x630000 +
   0x9D0000 (16 MB).
4. `S0_BRINGUP.md`: S0.1 → health check Freenove + display (pinos 21/45/47);
   S0.2 → spikes F-A/F-B; S0.3 ganha câmera DVP.
5. ROADMAP: S5 (visão) pode voltar ao escopo (câmera grátis); S6.2 volta a
   ser servo 1 fio (ou fallback 19/20).
6. `nb_hw_config.h` do firmware: mapa da §1.
7. Decidir o destino do v1 (a placa é a mesma que o roda).
