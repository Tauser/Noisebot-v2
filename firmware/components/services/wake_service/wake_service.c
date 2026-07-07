#include "wake_service.h"

#include <stddef.h>

static void nb_wake_output_clear(nb_wake_output_t *out)
{
    if (out == NULL) {
        return;
    }

    out->action = NB_WAKE_ACTION_NONE;
    out->session_id = 0u;
    out->samples = 0u;
    out->end_reason = NB_WAKE_END_REASON_NONE;
    out->feedback = NB_WAKE_FEEDBACK_NONE;
}

static bool nb_wake_has_route(const nb_wake_service_t *svc)
{
    return svc != NULL && (svc->mind_connected || svc->local_intent_available);
}

static void nb_wake_reset_session(nb_wake_service_t *svc)
{
    if (svc == NULL) {
        return;
    }

    svc->state = NB_WAKE_STATE_IDLE;
    svc->active_session_id = 0u;
    svc->armed_at_ms = 0u;
    svc->listening_started_at_ms = 0u;
    svc->last_voice_at_ms = 0u;
    svc->audio_samples_sent = 0u;
}

static bool nb_wake_emit_feedback(nb_wake_service_t *svc, nb_wake_feedback_t feedback,
                                  nb_wake_output_t *out)
{
    if (svc == NULL) {
        return false;
    }

    nb_wake_output_clear(out);
    if (out != NULL) {
        out->action = NB_WAKE_ACTION_FEEDBACK;
        out->feedback = feedback;
        out->session_id = svc->active_session_id;
    }
    nb_wake_reset_session(svc);
    return out != NULL;
}

static bool nb_wake_emit_listen_end(nb_wake_service_t *svc, nb_wake_end_reason_t reason,
                                    nb_wake_output_t *out)
{
    if (svc == NULL || svc->state != NB_WAKE_STATE_LISTENING ||
        svc->audio_samples_sent == 0u) {
        return false;
    }

    nb_wake_output_clear(out);
    if (out != NULL) {
        out->action = NB_WAKE_ACTION_LISTEN_END;
        out->session_id = svc->active_session_id;
        out->end_reason = reason;
        out->samples = svc->audio_samples_sent;
    }
    nb_wake_reset_session(svc);
    return out != NULL;
}

void nb_wake_service_init(nb_wake_service_t *svc)
{
    if (svc == NULL) {
        return;
    }

    svc->state = NB_WAKE_STATE_IDLE;
    svc->next_session_id = 1u;
    svc->active_session_id = 0u;
    svc->armed_at_ms = 0u;
    svc->listening_started_at_ms = 0u;
    svc->last_voice_at_ms = 0u;
    svc->audio_samples_sent = 0u;
    svc->vad_available = false;
    svc->mind_connected = false;
    svc->local_intent_available = false;
}

void nb_wake_service_set_vad_available(nb_wake_service_t *svc, bool available)
{
    if (svc == NULL) {
        return;
    }

    svc->vad_available = available;
}

void nb_wake_service_set_routes(nb_wake_service_t *svc, bool mind_connected,
                                bool local_intent_available)
{
    if (svc == NULL) {
        return;
    }

    svc->mind_connected = mind_connected;
    svc->local_intent_available = local_intent_available;
}

bool nb_wake_service_on_touch(nb_wake_service_t *svc, nb_wake_output_t *out)
{
    (void)svc;
    nb_wake_output_clear(out);
    return false;
}

bool nb_wake_service_on_wake(nb_wake_service_t *svc, uint32_t now_ms,
                             nb_wake_output_t *out)
{
    if (svc == NULL) {
        nb_wake_output_clear(out);
        return false;
    }

    nb_wake_output_clear(out);
    nb_wake_reset_session(svc);

    if (!svc->vad_available) {
        return nb_wake_emit_feedback(svc, NB_WAKE_FEEDBACK_VAD_UNAVAILABLE, out);
    }
    if (!nb_wake_has_route(svc)) {
        return nb_wake_emit_feedback(svc, NB_WAKE_FEEDBACK_NO_ROUTE, out);
    }

    svc->state = NB_WAKE_STATE_ARMED;
    svc->active_session_id = svc->next_session_id++;
    svc->armed_at_ms = now_ms;

    if (out != NULL) {
        out->action = NB_WAKE_ACTION_SESSION_ARMED;
        out->session_id = svc->active_session_id;
    }
    return out != NULL;
}

bool nb_wake_service_on_audio_frame(nb_wake_service_t *svc, uint32_t now_ms,
                                    bool vad_detected_speech, uint32_t samples,
                                    nb_wake_output_t *out)
{
    if (svc == NULL) {
        nb_wake_output_clear(out);
        return false;
    }

    nb_wake_output_clear(out);

    if ((svc->state == NB_WAKE_STATE_ARMED || svc->state == NB_WAKE_STATE_LISTENING) &&
        !nb_wake_has_route(svc)) {
        return nb_wake_emit_feedback(svc, NB_WAKE_FEEDBACK_NO_ROUTE, out);
    }

    if (!vad_detected_speech || samples == 0u) {
        return false;
    }

    if (svc->state == NB_WAKE_STATE_ARMED) {
        svc->state = NB_WAKE_STATE_LISTENING;
        svc->listening_started_at_ms = now_ms;
        svc->last_voice_at_ms = now_ms;
        svc->audio_samples_sent = samples;
        if (out != NULL) {
            out->action = NB_WAKE_ACTION_LISTEN_BEGIN_WITH_AUDIO;
            out->session_id = svc->active_session_id;
            out->samples = samples;
        }
        return out != NULL;
    }

    if (svc->state != NB_WAKE_STATE_LISTENING) {
        return false;
    }

    svc->last_voice_at_ms = now_ms;
    svc->audio_samples_sent += samples;
    if (out != NULL) {
        out->action = NB_WAKE_ACTION_LISTEN_AUDIO;
        out->session_id = svc->active_session_id;
        out->samples = samples;
    }
    return out != NULL;
}

bool nb_wake_service_tick(nb_wake_service_t *svc, uint32_t now_ms,
                          nb_wake_output_t *out)
{
    if (svc == NULL) {
        nb_wake_output_clear(out);
        return false;
    }

    nb_wake_output_clear(out);

    if ((svc->state == NB_WAKE_STATE_ARMED || svc->state == NB_WAKE_STATE_LISTENING) &&
        !nb_wake_has_route(svc)) {
        return nb_wake_emit_feedback(svc, NB_WAKE_FEEDBACK_NO_ROUTE, out);
    }

    if (svc->state != NB_WAKE_STATE_LISTENING) {
        return false;
    }

    if (svc->audio_samples_sent >= NB_WAKE_SERVICE_MAX_UTTERANCE_SAMPLES ||
        now_ms - svc->listening_started_at_ms >= NB_WAKE_SERVICE_MAX_UTTERANCE_MS) {
        return nb_wake_emit_listen_end(svc, NB_WAKE_END_REASON_MAX_DURATION, out);
    }

    if (now_ms - svc->last_voice_at_ms >= NB_WAKE_SERVICE_SILENCE_END_MS) {
        return nb_wake_emit_listen_end(svc, NB_WAKE_END_REASON_SILENCE, out);
    }

    return false;
}

nb_wake_state_t nb_wake_service_get_state(const nb_wake_service_t *svc)
{
    if (svc == NULL) {
        return NB_WAKE_STATE_IDLE;
    }

    return svc->state;
}
