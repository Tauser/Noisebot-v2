#ifndef NB_AUDIO_PLAYBACK_SERVICE_H
#define NB_AUDIO_PLAYBACK_SERVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro do audio_playback_service (S4.3, camada L3, VOICE.md §5).
 *
 * Responsabilidades desta fatia:
 * - receber `SAY_BEGIN/SAY_AUDIO/SAY_END` por turn_id;
 * - manter ring fixo injetado pela casca (PSRAM no firmware real);
 * - drenar áudio em ordem para o futuro writer I2S;
 * - cancelar/derrubar playback sem click duro, expondo um fade curto
 *   determinístico para a casca consumir.
 *
 * Sem ESP-IDF/FreeRTOS/I2S/event_bus: a casca futura injeta os frames
 * do `mind_link` e puxa amostras para o speaker.
 */

#define NB_AUDIO_PLAYBACK_DEFAULT_FADE_SAMPLES 4800u /* 300 ms @ 16 kHz */

typedef enum {
    NB_AUDIO_PLAYBACK_STATE_IDLE = 0,
    NB_AUDIO_PLAYBACK_STATE_BUFFERING = 1,
    NB_AUDIO_PLAYBACK_STATE_DRAINING = 2,
    NB_AUDIO_PLAYBACK_STATE_FADING = 3,
} nb_audio_playback_state_t;

typedef enum {
    NB_AUDIO_PLAYBACK_STOP_NONE = 0,
    NB_AUDIO_PLAYBACK_STOP_CANCELLED = 1,
    NB_AUDIO_PLAYBACK_STOP_SERVER_DROPPED = 2,
} nb_audio_playback_stop_reason_t;

typedef struct {
    nb_audio_playback_state_t state;
    nb_audio_playback_stop_reason_t stop_reason;
    uint32_t active_turn_id;
    uint32_t sample_rate;
    int16_t *ring;
    size_t ring_capacity_samples;
    size_t ring_head;
    size_t ring_tail;
    size_t ring_count;
    size_t received_samples;
    size_t played_samples;
    size_t dropped_samples;
    size_t fade_total_samples;
    size_t fade_remaining_samples;
    int16_t last_output_sample;
} nb_audio_playback_service_t;

bool nb_audio_playback_service_init(nb_audio_playback_service_t *svc, int16_t *ring_storage,
                                    size_t ring_capacity_samples, size_t fade_samples);

void nb_audio_playback_service_reset(nb_audio_playback_service_t *svc);

bool nb_audio_playback_service_on_say_begin(nb_audio_playback_service_t *svc, uint32_t turn_id,
                                            uint32_t sample_rate);

size_t nb_audio_playback_service_on_say_audio(nb_audio_playback_service_t *svc, uint32_t turn_id,
                                              const int16_t *samples, size_t sample_count);

bool nb_audio_playback_service_on_say_end(nb_audio_playback_service_t *svc, uint32_t turn_id);

bool nb_audio_playback_service_on_say_cancel(nb_audio_playback_service_t *svc, uint32_t turn_id);

bool nb_audio_playback_service_on_server_drop(nb_audio_playback_service_t *svc);

size_t nb_audio_playback_service_consume(nb_audio_playback_service_t *svc, int16_t *out_samples,
                                         size_t max_samples);

nb_audio_playback_state_t
nb_audio_playback_service_get_state(const nb_audio_playback_service_t *svc);

nb_audio_playback_stop_reason_t
nb_audio_playback_service_get_stop_reason(const nb_audio_playback_service_t *svc);

size_t nb_audio_playback_service_get_buffered_samples(const nb_audio_playback_service_t *svc);

#ifdef __cplusplus
}
#endif

#endif
