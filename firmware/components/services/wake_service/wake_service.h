#ifndef NB_WAKE_SERVICE_H
#define NB_WAKE_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro do wake_service (S4.2, camada L3, VOICE.md §2-4).
 *
 * Responsabilidades desta fatia:
 * - só wake word abre uma sessão de escuta;
 * - VAD fora de sessão nunca publica atividade;
 * - LISTEN_START/END obedecem os invariantes V-2/V-4;
 * - ausência de VAD ou de rota útil (mente/local) produz feedback honesto.
 *
 * Sem ESP-SR/ESP-IDF/FreeRTOS/event_bus: a casca futura injeta wake,
 * frames classificados pelo VAD real e o estado da conexão da mente.
 */

#define NB_WAKE_SERVICE_SAMPLE_RATE 16000u
#define NB_WAKE_SERVICE_SILENCE_END_MS 900u
#define NB_WAKE_SERVICE_MAX_UTTERANCE_MS 9200u
#define NB_WAKE_SERVICE_MIN_UTTERANCE_SAMPLES 8000u
#define NB_WAKE_SERVICE_MAX_UTTERANCE_SAMPLES 192000u
#define NB_WAKE_SERVICE_CHUNK_SAMPLES 256u

typedef enum {
    NB_WAKE_STATE_IDLE = 0,
    NB_WAKE_STATE_ARMED = 1,
    NB_WAKE_STATE_LISTENING = 2,
} nb_wake_state_t;

typedef enum {
    NB_WAKE_ACTION_NONE = 0,
    NB_WAKE_ACTION_SESSION_ARMED = 1,
    NB_WAKE_ACTION_LISTEN_BEGIN_WITH_AUDIO = 2,
    NB_WAKE_ACTION_LISTEN_AUDIO = 3,
    NB_WAKE_ACTION_LISTEN_END = 4,
    NB_WAKE_ACTION_FEEDBACK = 5,
} nb_wake_action_t;

typedef enum {
    NB_WAKE_END_REASON_NONE = 0,
    NB_WAKE_END_REASON_SILENCE = 1,
    NB_WAKE_END_REASON_MAX_DURATION = 2,
} nb_wake_end_reason_t;

typedef enum {
    NB_WAKE_FEEDBACK_NONE = 0,
    NB_WAKE_FEEDBACK_VAD_UNAVAILABLE = 1,
    NB_WAKE_FEEDBACK_NO_ROUTE = 2,
} nb_wake_feedback_t;

typedef struct {
    nb_wake_action_t action;
    uint32_t session_id;
    uint32_t samples;
    nb_wake_end_reason_t end_reason;
    nb_wake_feedback_t feedback;
} nb_wake_output_t;

typedef struct {
    nb_wake_state_t state;
    uint32_t next_session_id;
    uint32_t active_session_id;
    uint32_t armed_at_ms;
    uint32_t listening_started_at_ms;
    uint32_t last_voice_at_ms;
    uint32_t audio_samples_sent;
    bool vad_available;
    bool mind_connected;
    bool local_intent_available;
} nb_wake_service_t;

void nb_wake_service_init(nb_wake_service_t *svc);

void nb_wake_service_set_vad_available(nb_wake_service_t *svc, bool available);

void nb_wake_service_set_routes(nb_wake_service_t *svc, bool mind_connected,
                                bool local_intent_available);

bool nb_wake_service_on_touch(nb_wake_service_t *svc, nb_wake_output_t *out);

bool nb_wake_service_on_wake(nb_wake_service_t *svc, uint32_t now_ms,
                             nb_wake_output_t *out);

bool nb_wake_service_on_audio_frame(nb_wake_service_t *svc, uint32_t now_ms,
                                    bool vad_detected_speech, uint32_t samples,
                                    nb_wake_output_t *out);

bool nb_wake_service_tick(nb_wake_service_t *svc, uint32_t now_ms,
                          nb_wake_output_t *out);

nb_wake_state_t nb_wake_service_get_state(const nb_wake_service_t *svc);

#ifdef __cplusplus
}
#endif

#endif
