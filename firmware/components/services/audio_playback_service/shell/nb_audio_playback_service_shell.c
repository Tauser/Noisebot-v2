#include "nb_audio_playback_service_shell.h"

#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#define NB_AUDIO_PLAYBACK_SHELL_RING_SAMPLES 8192u
#define NB_AUDIO_PLAYBACK_SHELL_CONVERT_SAMPLES 512u

static const char *TAG = "audio_playback";
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static bool s_initialized;
static nb_audio_playback_service_t s_service;
static int16_t s_ring_storage[NB_AUDIO_PLAYBACK_SHELL_RING_SAMPLES];
static int16_t s_convert_buf[NB_AUDIO_PLAYBACK_SHELL_CONVERT_SAMPLES];

static size_t s_total_dropped_samples;
static uint32_t s_volume_percent = 100u;

static const char *nb_audio_playback_service_shell_state_name(nb_audio_playback_state_t state)
{
    switch (state) {
    case NB_AUDIO_PLAYBACK_STATE_BUFFERING:
        return "BUFFERING";
    case NB_AUDIO_PLAYBACK_STATE_DRAINING:
        return "DRAINING";
    case NB_AUDIO_PLAYBACK_STATE_FADING:
        return "FADING";
    case NB_AUDIO_PLAYBACK_STATE_IDLE:
    default:
        return "IDLE";
    }
}

static bool nb_audio_playback_service_shell_ensure_init(void)
{
    if (s_initialized) {
        return true;
    }
    return nb_audio_playback_service_shell_init() == ESP_OK;
}

esp_err_t nb_audio_playback_service_shell_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    if (!nb_audio_playback_service_init(&s_service, s_ring_storage,
                                        NB_AUDIO_PLAYBACK_SHELL_RING_SAMPLES,
                                        NB_AUDIO_PLAYBACK_DEFAULT_FADE_SAMPLES)) {
        return ESP_FAIL;
    }

    s_total_dropped_samples = 0u;
    s_volume_percent = 100u;
    s_initialized = true;
    return ESP_OK;
}

bool nb_audio_playback_service_shell_on_say_begin(uint32_t turn_id, uint32_t sample_rate)
{
    bool ok;

    if (!nb_audio_playback_service_shell_ensure_init()) {
        return false;
    }

    portENTER_CRITICAL(&s_mux);
    ok = nb_audio_playback_service_on_say_begin(&s_service, turn_id, sample_rate);
    portEXIT_CRITICAL(&s_mux);
    if (ok) {
        ESP_LOGI(TAG, "SAY_BEGIN local turn=%u sample_rate=%u state=%s",
                 (unsigned)turn_id, (unsigned)sample_rate,
                 nb_audio_playback_service_shell_state_name(
                     nb_audio_playback_service_shell_get_state()));
    }
    return ok;
}

size_t nb_audio_playback_service_shell_on_say_audio(uint32_t turn_id, const uint8_t *pcm_bytes,
                                                    size_t pcm_len_bytes)
{
    size_t samples;
    size_t i;
    size_t written;

    if (!nb_audio_playback_service_shell_ensure_init() || pcm_bytes == NULL ||
        pcm_len_bytes == 0u || (pcm_len_bytes % sizeof(int16_t)) != 0u) {
        return 0u;
    }

    samples = pcm_len_bytes / sizeof(int16_t);
    if (samples > NB_AUDIO_PLAYBACK_SHELL_CONVERT_SAMPLES) {
        return 0u;
    }

    /* Payload do protocolo é bytes; a casca faz a tradução explícita para
     * PCM16 little-endian antes de entregar ao núcleo puro. */
    for (i = 0u; i < samples; ++i) {
        const uint16_t lo = pcm_bytes[i * 2u];
        const uint16_t hi = pcm_bytes[i * 2u + 1u];
        s_convert_buf[i] = (int16_t)(lo | (hi << 8u));
    }

    portENTER_CRITICAL(&s_mux);
    written = nb_audio_playback_service_on_say_audio(&s_service, turn_id, s_convert_buf, samples);
    s_total_dropped_samples += samples - written;
    portEXIT_CRITICAL(&s_mux);

    if (written < samples) {
        ESP_LOGW(TAG, "drop de playback turn=%u wrote=%u dropped=%u buffered=%u",
                 (unsigned)turn_id, (unsigned)written, (unsigned)(samples - written),
                 (unsigned)nb_audio_playback_service_get_buffered_samples(&s_service));
    } else {
        ESP_LOGI(TAG, "SAY_AUDIO local turn=%u samples=%u buffered=%u state=%s",
                 (unsigned)turn_id, (unsigned)samples,
                 (unsigned)nb_audio_playback_service_shell_get_buffered_samples(),
                 nb_audio_playback_service_shell_state_name(
                     nb_audio_playback_service_shell_get_state()));
    }
    return written;
}

bool nb_audio_playback_service_shell_on_say_end(uint32_t turn_id)
{
    bool ok;

    if (!nb_audio_playback_service_shell_ensure_init()) {
        return false;
    }

    portENTER_CRITICAL(&s_mux);
    ok = nb_audio_playback_service_on_say_end(&s_service, turn_id);
    portEXIT_CRITICAL(&s_mux);
    if (ok) {
        ESP_LOGI(TAG, "SAY_END local turn=%u state=%s buffered=%u",
                 (unsigned)turn_id,
                 nb_audio_playback_service_shell_state_name(
                     nb_audio_playback_service_shell_get_state()),
                 (unsigned)nb_audio_playback_service_shell_get_buffered_samples());
    }
    return ok;
}

bool nb_audio_playback_service_shell_on_say_cancel(uint32_t turn_id)
{
    bool ok;

    if (!nb_audio_playback_service_shell_ensure_init()) {
        return false;
    }

    portENTER_CRITICAL(&s_mux);
    ok = nb_audio_playback_service_on_say_cancel(&s_service, turn_id);
    portEXIT_CRITICAL(&s_mux);
    if (ok) {
        ESP_LOGW(TAG, "SAY_CANCEL local turn=%u state=%s",
                 (unsigned)turn_id,
                 nb_audio_playback_service_shell_state_name(
                     nb_audio_playback_service_shell_get_state()));
    }
    return ok;
}

bool nb_audio_playback_service_shell_on_server_drop(void)
{
    bool ok;

    if (!nb_audio_playback_service_shell_ensure_init()) {
        return false;
    }

    portENTER_CRITICAL(&s_mux);
    ok = nb_audio_playback_service_on_server_drop(&s_service);
    portEXIT_CRITICAL(&s_mux);
    if (ok) {
        ESP_LOGW(TAG, "server_drop local state=%s buffered=%u dropped_total=%u",
                 nb_audio_playback_service_shell_state_name(
                     nb_audio_playback_service_shell_get_state()),
                 (unsigned)nb_audio_playback_service_shell_get_buffered_samples(),
                 (unsigned)s_total_dropped_samples);
    }
    return ok;
}

size_t nb_audio_playback_service_shell_consume(int16_t *out_samples, size_t max_samples)
{
    size_t consumed;
    uint32_t volume_percent;
    size_t i;

    if (!nb_audio_playback_service_shell_ensure_init()) {
        return 0u;
    }

    portENTER_CRITICAL(&s_mux);
    consumed = nb_audio_playback_service_consume(&s_service, out_samples, max_samples);
    volume_percent = s_volume_percent;
    portEXIT_CRITICAL(&s_mux);

    if (volume_percent == 100u || consumed == 0u) {
        return consumed;
    }

    if (volume_percent == 0u) {
        for (i = 0u; i < consumed; ++i) {
            out_samples[i] = 0;
        }
        return consumed;
    }

    for (i = 0u; i < consumed; ++i) {
        const int32_t scaled = ((int32_t)out_samples[i] * (int32_t)volume_percent) / 100;
        if (scaled > 32767) {
            out_samples[i] = 32767;
        } else if (scaled < -32768) {
            out_samples[i] = -32768;
        } else {
            out_samples[i] = (int16_t)scaled;
        }
    }
    return consumed;
}

bool nb_audio_playback_service_shell_set_volume_percent(uint32_t volume_percent)
{
    if (volume_percent > 100u) {
        return false;
    }

    if (!nb_audio_playback_service_shell_ensure_init()) {
        return false;
    }

    portENTER_CRITICAL(&s_mux);
    s_volume_percent = volume_percent;
    portEXIT_CRITICAL(&s_mux);
    ESP_LOGI(TAG, "volume local ajustado para %u%%", (unsigned)volume_percent);
    return true;
}

uint32_t nb_audio_playback_service_shell_get_volume_percent(void)
{
    uint32_t volume_percent;

    if (!nb_audio_playback_service_shell_ensure_init()) {
        return 100u;
    }

    portENTER_CRITICAL(&s_mux);
    volume_percent = s_volume_percent;
    portEXIT_CRITICAL(&s_mux);
    return volume_percent;
}

nb_audio_playback_state_t nb_audio_playback_service_shell_get_state(void)
{
    nb_audio_playback_state_t state;

    if (!nb_audio_playback_service_shell_ensure_init()) {
        return NB_AUDIO_PLAYBACK_STATE_IDLE;
    }

    portENTER_CRITICAL(&s_mux);
    state = nb_audio_playback_service_get_state(&s_service);
    portEXIT_CRITICAL(&s_mux);
    return state;
}

size_t nb_audio_playback_service_shell_get_buffered_samples(void)
{
    size_t count;

    if (!nb_audio_playback_service_shell_ensure_init()) {
        return 0u;
    }

    portENTER_CRITICAL(&s_mux);
    count = nb_audio_playback_service_get_buffered_samples(&s_service);
    portEXIT_CRITICAL(&s_mux);
    return count;
}

size_t nb_audio_playback_service_shell_get_dropped_samples(void)
{
    return s_total_dropped_samples;
}
