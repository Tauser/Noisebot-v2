#ifndef NB_AUDIO_HAL_H
#define NB_AUDIO_HAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro do audio_hal (S4.1, camada L0, HARDWARE.md). O barramento
 * I2S em si (full-duplex real, INMP441+MAX98357A) é hardware puro -- sem
 * lógica não-trivial pra extrair além do cálculo de nível de sinal, então
 * o núcleo aqui é só isso: RMS de um bloco PCM16, reaproveitável tanto
 * pelo ensaio de loopback deste componente quanto pela análise leve de
 * nível de `wake_service` (S4.2, VOICE.md §2 -- "RMS/ZCR só diagnóstico,
 * nunca abre sessão").
 *
 * Ring buffer/backpressure de playback (VOICE.md §5) é `audio_service`
 * (L3), fora de escopo de S4.1.
 */

/* RMS (root mean square) de count amostras PCM16 -- 0.0 se samples for
 * NULL ou count for 0. */
float nb_audio_hal_rms_s16(const int16_t *samples, size_t count);

#ifdef __cplusplus
}
#endif

#endif
