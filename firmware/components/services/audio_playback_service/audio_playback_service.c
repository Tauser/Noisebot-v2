#include "audio_playback_service.h"

static void nb_audio_playback_service_clear_runtime(nb_audio_playback_service_t *svc)
{
    if (svc == NULL) {
        return;
    }

    svc->state = NB_AUDIO_PLAYBACK_STATE_IDLE;
    svc->stop_reason = NB_AUDIO_PLAYBACK_STOP_NONE;
    svc->active_turn_id = 0u;
    svc->sample_rate = 0u;
    svc->ring_head = 0u;
    svc->ring_tail = 0u;
    svc->ring_count = 0u;
    svc->received_samples = 0u;
    svc->played_samples = 0u;
    svc->dropped_samples = 0u;
    svc->fade_remaining_samples = 0u;
    svc->last_output_sample = 0;
}

static bool nb_audio_playback_service_has_active_turn(const nb_audio_playback_service_t *svc,
                                                      uint32_t turn_id)
{
    return svc != NULL && turn_id != 0u && svc->active_turn_id == turn_id &&
           (svc->state == NB_AUDIO_PLAYBACK_STATE_BUFFERING ||
            svc->state == NB_AUDIO_PLAYBACK_STATE_DRAINING);
}

static void nb_audio_playback_service_start_fade(nb_audio_playback_service_t *svc,
                                                 nb_audio_playback_stop_reason_t reason)
{
    if (svc == NULL) {
        return;
    }

    svc->ring_head = 0u;
    svc->ring_tail = 0u;
    svc->ring_count = 0u;
    svc->received_samples = 0u;
    svc->played_samples = 0u;
    svc->dropped_samples = 0u;
    svc->stop_reason = reason;

    if (svc->fade_total_samples == 0u || svc->last_output_sample == 0) {
        svc->state = NB_AUDIO_PLAYBACK_STATE_IDLE;
        svc->active_turn_id = 0u;
        svc->sample_rate = 0u;
        svc->fade_remaining_samples = 0u;
        return;
    }

    svc->state = NB_AUDIO_PLAYBACK_STATE_FADING;
    svc->active_turn_id = 0u;
    svc->sample_rate = 0u;
    svc->fade_remaining_samples = svc->fade_total_samples;
}

bool nb_audio_playback_service_init(nb_audio_playback_service_t *svc, int16_t *ring_storage,
                                    size_t ring_capacity_samples, size_t fade_samples)
{
    if (svc == NULL || ring_storage == NULL || ring_capacity_samples == 0u) {
        return false;
    }

    svc->ring = ring_storage;
    svc->ring_capacity_samples = ring_capacity_samples;
    svc->fade_total_samples = fade_samples == 0u ? NB_AUDIO_PLAYBACK_DEFAULT_FADE_SAMPLES : fade_samples;
    nb_audio_playback_service_clear_runtime(svc);
    return true;
}

void nb_audio_playback_service_reset(nb_audio_playback_service_t *svc)
{
    if (svc == NULL) {
        return;
    }

    nb_audio_playback_service_clear_runtime(svc);
}

bool nb_audio_playback_service_on_say_begin(nb_audio_playback_service_t *svc, uint32_t turn_id,
                                            uint32_t sample_rate)
{
    if (svc == NULL || turn_id == 0u || sample_rate == 0u) {
        return false;
    }

    nb_audio_playback_service_clear_runtime(svc);
    svc->state = NB_AUDIO_PLAYBACK_STATE_BUFFERING;
    svc->active_turn_id = turn_id;
    svc->sample_rate = sample_rate;
    return true;
}

size_t nb_audio_playback_service_on_say_audio(nb_audio_playback_service_t *svc, uint32_t turn_id,
                                              const int16_t *samples, size_t sample_count)
{
    size_t written = 0u;

    if (!nb_audio_playback_service_has_active_turn(svc, turn_id) || samples == NULL ||
        sample_count == 0u || svc->state != NB_AUDIO_PLAYBACK_STATE_BUFFERING) {
        return 0u;
    }

    while (written < sample_count && svc->ring_count < svc->ring_capacity_samples) {
        svc->ring[svc->ring_tail] = samples[written];
        svc->ring_tail = (svc->ring_tail + 1u) % svc->ring_capacity_samples;
        ++svc->ring_count;
        ++written;
    }

    svc->received_samples += written;
    svc->dropped_samples += sample_count - written;
    return written;
}

bool nb_audio_playback_service_on_say_end(nb_audio_playback_service_t *svc, uint32_t turn_id)
{
    if (!nb_audio_playback_service_has_active_turn(svc, turn_id)) {
        return false;
    }

    if (svc->ring_count == 0u) {
        nb_audio_playback_service_clear_runtime(svc);
        return true;
    }

    svc->state = NB_AUDIO_PLAYBACK_STATE_DRAINING;
    return true;
}

bool nb_audio_playback_service_on_say_cancel(nb_audio_playback_service_t *svc, uint32_t turn_id)
{
    if (!nb_audio_playback_service_has_active_turn(svc, turn_id)) {
        return false;
    }

    nb_audio_playback_service_start_fade(svc, NB_AUDIO_PLAYBACK_STOP_CANCELLED);
    return true;
}

bool nb_audio_playback_service_on_server_drop(nb_audio_playback_service_t *svc)
{
    if (svc == NULL || (svc->state != NB_AUDIO_PLAYBACK_STATE_BUFFERING &&
                        svc->state != NB_AUDIO_PLAYBACK_STATE_DRAINING)) {
        return false;
    }

    if (svc->ring_count > 0u) {
        /* Se o link cair com áudio já enfileirado, ainda drena esse trecho
         * curto antes de aplicar o fade de saída; assim a bancada vê
         * "queda no meio da fala" em vez de silêncio abrupto. */
        svc->state = NB_AUDIO_PLAYBACK_STATE_DRAINING;
        svc->stop_reason = NB_AUDIO_PLAYBACK_STOP_SERVER_DROPPED;
        svc->active_turn_id = 0u;
        svc->sample_rate = 0u;
        return true;
    }

    nb_audio_playback_service_start_fade(svc, NB_AUDIO_PLAYBACK_STOP_SERVER_DROPPED);
    return true;
}

size_t nb_audio_playback_service_consume(nb_audio_playback_service_t *svc, int16_t *out_samples,
                                         size_t max_samples)
{
    size_t produced = 0u;

    if (svc == NULL || out_samples == NULL || max_samples == 0u) {
        return 0u;
    }

    while (produced < max_samples) {
        if (svc->ring_count > 0u &&
            (svc->state == NB_AUDIO_PLAYBACK_STATE_BUFFERING ||
             svc->state == NB_AUDIO_PLAYBACK_STATE_DRAINING)) {
            out_samples[produced] = svc->ring[svc->ring_head];
            svc->last_output_sample = out_samples[produced];
            svc->ring_head = (svc->ring_head + 1u) % svc->ring_capacity_samples;
            --svc->ring_count;
            ++svc->played_samples;
            ++produced;
            continue;
        }

        if (svc->state == NB_AUDIO_PLAYBACK_STATE_FADING && svc->fade_remaining_samples > 0u) {
            const int32_t numerator =
                (int32_t)svc->last_output_sample * (int32_t)svc->fade_remaining_samples;
            const int32_t denominator = (int32_t)svc->fade_total_samples;
            out_samples[produced] = (int16_t)(numerator / denominator);
            --svc->fade_remaining_samples;
            ++produced;
            if (svc->fade_remaining_samples == 0u) {
                svc->last_output_sample = 0;
                svc->state = NB_AUDIO_PLAYBACK_STATE_IDLE;
                svc->stop_reason = NB_AUDIO_PLAYBACK_STOP_NONE;
            }
            continue;
        }

        break;
    }

    if (svc->state == NB_AUDIO_PLAYBACK_STATE_DRAINING && svc->ring_count == 0u) {
        if (svc->stop_reason == NB_AUDIO_PLAYBACK_STOP_SERVER_DROPPED) {
            nb_audio_playback_service_start_fade(svc, NB_AUDIO_PLAYBACK_STOP_SERVER_DROPPED);
        } else {
            svc->state = NB_AUDIO_PLAYBACK_STATE_IDLE;
            svc->stop_reason = NB_AUDIO_PLAYBACK_STOP_NONE;
            svc->active_turn_id = 0u;
            svc->sample_rate = 0u;
        }
    }

    return produced;
}

nb_audio_playback_state_t
nb_audio_playback_service_get_state(const nb_audio_playback_service_t *svc)
{
    return svc == NULL ? NB_AUDIO_PLAYBACK_STATE_IDLE : svc->state;
}

nb_audio_playback_stop_reason_t
nb_audio_playback_service_get_stop_reason(const nb_audio_playback_service_t *svc)
{
    return svc == NULL ? NB_AUDIO_PLAYBACK_STOP_NONE : svc->stop_reason;
}

size_t nb_audio_playback_service_get_buffered_samples(const nb_audio_playback_service_t *svc)
{
    return svc == NULL ? 0u : svc->ring_count;
}
