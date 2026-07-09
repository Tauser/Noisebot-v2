#ifndef NB_AUDIO_PLAYBACK_SERVICE_SHELL_H
#define NB_AUDIO_PLAYBACK_SERVICE_SHELL_H

#include <stddef.h>
#include <stdint.h>

#include "audio_playback_service.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Casca mínima do audio_playback_service (S4.3): dona da instância única do
 * núcleo, do ring estático e da tradução bytes->PCM16 vinda do protocolo.
 *
 * Nesta fatia ainda não cria task própria nem escreve no I2S; ela só recebe
 * o downlink `SAY_*` do `mind_link` e deixa o estado pronto para a próxima
 * integração com o writer do `audio_hal`.
 */

esp_err_t nb_audio_playback_service_shell_init(void);

bool nb_audio_playback_service_shell_on_say_begin(uint32_t turn_id, uint32_t sample_rate);
size_t nb_audio_playback_service_shell_on_say_audio(uint32_t turn_id, const uint8_t *pcm_bytes,
                                                    size_t pcm_len_bytes);
bool nb_audio_playback_service_shell_on_say_end(uint32_t turn_id);
bool nb_audio_playback_service_shell_on_say_cancel(uint32_t turn_id);
bool nb_audio_playback_service_shell_on_server_drop(void);

size_t nb_audio_playback_service_shell_consume(int16_t *out_samples, size_t max_samples);
nb_audio_playback_state_t nb_audio_playback_service_shell_get_state(void);
size_t nb_audio_playback_service_shell_get_buffered_samples(void);
size_t nb_audio_playback_service_shell_get_dropped_samples(void);

#ifdef __cplusplus
}
#endif

#endif
