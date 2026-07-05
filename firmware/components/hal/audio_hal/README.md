# audio_hal

Componente `firmware/components/hal/audio_hal` — S4.1, camada L0
(`ARCHITECTURE.md`, `HARDWARE.md`). Núcleo C17 puro (`audio_hal.c/.h`) é só
`nb_audio_hal_rms_s16()` — RMS de um bloco PCM16, sem lógica não-trivial
adicional pra extrair (I2S em si é hardware puro), reaproveitável tanto
pelo ensaio de loopback deste componente quanto pela análise leve de nível
de `wake_service` (S4.2, VOICE.md §2: "RMS/ZCR só diagnóstico, nunca abre
sessão").

`shell/nb_audio_hal_shell.c/.h`: I2S full-duplex real via `driver/i2s_std.h`
(ESP-IDF v5.5) — canais TX e RX alocados no mesmo `I2S_NUM_0`
(`i2s_new_channel` com os dois handles não-nulos forma full-duplex
automaticamente, BCLK/WS compartilhados). INMP441 (mic, RX/DIN GPIO39) +
MAX98357A (speaker, TX/DOUT GPIO42), BCLK GPIO40 / WS GPIO41
(`nb_hw_config.h`). Sem MCLK (nenhum dos dois precisa).

**Nota de bring-up:** no ESP32-S3, a macro `I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG`
fixa `slot_mask=I2S_STD_SLOT_BOTH` independente do argumento de modo
(mono/estéreo) — foi preciso sobrescrever `slot_mask` explicitamente pra
`I2S_STD_SLOT_LEFT` depois da macro, senão o INMP441 (que fala só um lado)
não seria lido corretamente.

"Underrun" não é um evento nomeado assim pelo driver ESP-IDF — os
contadores reais expostos são overflow de fila (RX: app não lê rápido o
bastante, dado descartado; TX: app escreve mais rápido do que a fila
aguenta) via `i2s_channel_register_event_callback`
(`on_recv_q_ovf`/`on_send_q_ovf`). Pra medir o gate ("zero underrun em
30 min") de forma honesta, os wrappers de `read`/`write` também contam
`ESP_ERR_TIMEOUT` -- sinal direto de que o app não acompanhou o ritmo do
DMA a tempo, o proxy mais próximo de "underrun" que dá pra medir num HAL
que só expõe leitura/escrita bloqueante.

Ring buffer/backpressure de playback (VOICE.md §5) é responsabilidade de
`audio_service` (L3), fora de escopo de S4.1.

Host-test cobre só o núcleo puro (RMS): silêncio (RMS=0), DC (RMS=valor
absoluto), senoide com número inteiro de períodos (RMS analítico =
amplitude/√2), `NULL`/`count=0` seguros. Sem host-test de I2S real (HAL de
hardware, mesma regra de `led_hal`).
