#ifndef NB_AUDIO_HAL_SHELL_H
#define NB_AUDIO_HAL_SHELL_H

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Casca do audio_hal (S4.1): I2S full-duplex real (`driver/i2s_std.h`,
 * ESP-IDF v5.5) -- canais TX+RX alocados no mesmo I2S_NUM_0
 * (`i2s_new_channel` com os dois handles não-nulos forma full-duplex
 * automaticamente, BCLK/WS compartilhados). INMP441 (mic, RX/DIN) +
 * MAX98357A (speaker, TX/DOUT), pinos em `nb_hw_config.h`.
 *
 * "Underrun" não é um evento nomeado assim pelo driver -- os contadores
 * reais são overflow de fila (RX: app não lê rápido o bastante, dado
 * descartado; TX: app escreve mais rápido do que a fila aguenta). Pra
 * medir o gate ("zero underrun em 30 min") de forma honesta, os
 * wrappers de read/write também contam timeout (`ESP_ERR_TIMEOUT`) --
 * sinal direto de que o app não acompanhou o ritmo do DMA a tempo.
 */

esp_err_t nb_audio_hal_shell_init(void);

/* Bloqueante até timeout_ms. out_read_count/out_written_count em
 * amostras (não bytes). */
esp_err_t nb_audio_hal_shell_read(int16_t *out_samples, size_t sample_count, uint32_t timeout_ms,
                                  size_t *out_read_count);
esp_err_t nb_audio_hal_shell_write(const int16_t *samples, size_t sample_count, uint32_t timeout_ms,
                                   size_t *out_written_count);

uint32_t nb_audio_hal_shell_get_rx_overflow_count(void);
uint32_t nb_audio_hal_shell_get_tx_overflow_count(void);
uint32_t nb_audio_hal_shell_get_rx_timeout_count(void);
uint32_t nb_audio_hal_shell_get_tx_timeout_count(void);

#ifdef __cplusplus
}
#endif

#endif
