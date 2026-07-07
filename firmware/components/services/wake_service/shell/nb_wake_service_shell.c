#include "nb_wake_service_shell.h"

#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "nb_event_bus_shell.h"

static const char *TAG = "wake_service";

static nb_wake_service_t s_svc;
static bool s_initialized;

static uint32_t nb_wake_service_shell_now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void nb_wake_service_shell_publish(nb_voice_event_kind_t kind, uint32_t session_id,
                                          uint32_t samples, float wake_score, uint8_t detail)
{
    nb_voice_event_payload_t payload = {
        .session_id = session_id,
        .samples = samples,
        .wake_score = wake_score,
        .kind = (uint8_t)kind,
        .detail = detail,
        .reserved = 0u,
    };
    nb_event_t bus_event = {
        .type = NB_EVENT_TYPE_VOICE,
        .priority = NB_EVENT_PRIORITY_NORMAL,
        .timestamp_ms = nb_wake_service_shell_now_ms(),
        .payload_len = (uint8_t)sizeof(payload),
    };

    memcpy(bus_event.payload, &payload, sizeof(payload));
    const nb_event_bus_status_t status = nb_event_bus_shell_publish(&bus_event);
    if (status != NB_EVENT_BUS_OK) {
        ESP_LOGW(TAG, "event_bus publish falhou (status=%d kind=%u)", (int)status, (unsigned)kind);
    }
}

static void nb_wake_service_shell_handle_output(const nb_wake_output_t *out, float wake_score)
{
    if (out == NULL) {
        return;
    }

    switch (out->action) {
    case NB_WAKE_ACTION_SESSION_ARMED:
        ESP_LOGI(TAG, "wake armado -- session=%u score=%.2f", (unsigned)out->session_id,
                 (double)wake_score);
        nb_wake_service_shell_publish(NB_VOICE_EVENT_WAKE, out->session_id, 0u, wake_score, 0u);
        break;
    case NB_WAKE_ACTION_LISTEN_BEGIN_WITH_AUDIO:
        ESP_LOGI(TAG, "listen_start -- session=%u samples=%u", (unsigned)out->session_id,
                 (unsigned)out->samples);
        nb_wake_service_shell_publish(NB_VOICE_EVENT_LISTEN_START, out->session_id, out->samples,
                                      0.0f, 0u);
        break;
    case NB_WAKE_ACTION_LISTEN_AUDIO:
        nb_wake_service_shell_publish(NB_VOICE_EVENT_LISTEN_AUDIO, out->session_id, out->samples,
                                      0.0f, 0u);
        break;
    case NB_WAKE_ACTION_LISTEN_END:
        ESP_LOGI(TAG, "listen_end -- session=%u reason=%u samples=%u", (unsigned)out->session_id,
                 (unsigned)out->end_reason, (unsigned)out->samples);
        nb_wake_service_shell_publish(NB_VOICE_EVENT_LISTEN_END, out->session_id, out->samples,
                                      0.0f, (uint8_t)out->end_reason);
        break;
    case NB_WAKE_ACTION_FEEDBACK:
        ESP_LOGW(TAG, "feedback honesto -- session=%u feedback=%u", (unsigned)out->session_id,
                 (unsigned)out->feedback);
        nb_wake_service_shell_publish(NB_VOICE_EVENT_FEEDBACK, out->session_id, 0u, 0.0f,
                                      (uint8_t)out->feedback);
        break;
    case NB_WAKE_ACTION_NONE:
    default:
        break;
    }
}

esp_err_t nb_wake_service_shell_init(void)
{
    if (s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    nb_wake_service_init(&s_svc);
    s_initialized = true;
    return ESP_OK;
}

void nb_wake_service_shell_set_vad_available(bool available)
{
    if (!s_initialized) {
        return;
    }

    nb_wake_service_set_vad_available(&s_svc, available);
}

void nb_wake_service_shell_set_routes(bool mind_connected, bool local_intent_available)
{
    if (!s_initialized) {
        return;
    }

    nb_wake_service_set_routes(&s_svc, mind_connected, local_intent_available);
}

void nb_wake_service_shell_trigger_wake(float score)
{
    nb_wake_output_t out;

    if (!s_initialized) {
        return;
    }

    if (score < 0.0f) {
        score = 0.0f;
    } else if (score > 1.0f) {
        score = 1.0f;
    }

    if (nb_wake_service_on_wake(&s_svc, nb_wake_service_shell_now_ms(), &out)) {
        nb_wake_service_shell_handle_output(&out, score);
    }
}

void nb_wake_service_shell_on_audio_frame(bool vad_detected_speech, uint32_t samples)
{
    nb_wake_output_t out;

    if (!s_initialized) {
        return;
    }

    if (nb_wake_service_on_audio_frame(&s_svc, nb_wake_service_shell_now_ms(),
                                       vad_detected_speech, samples, &out)) {
        nb_wake_service_shell_handle_output(&out, 0.0f);
    }
}

void nb_wake_service_shell_tick(void)
{
    nb_wake_output_t out;

    if (!s_initialized) {
        return;
    }

    if (nb_wake_service_tick(&s_svc, nb_wake_service_shell_now_ms(), &out)) {
        nb_wake_service_shell_handle_output(&out, 0.0f);
    }
}

nb_wake_state_t nb_wake_service_shell_get_state(void)
{
    return nb_wake_service_get_state(s_initialized ? &s_svc : NULL);
}
