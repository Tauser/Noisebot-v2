#include "../wake_service.h"

#include <stdio.h>

static int failures;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);           \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static void make_ready(nb_wake_service_t *svc)
{
    nb_wake_service_set_vad_available(svc, true);
    nb_wake_service_set_routes(svc, true, false);
}

static bool wake_and_arm(nb_wake_service_t *svc, uint32_t now_ms)
{
    nb_wake_output_t out;
    return nb_wake_service_on_wake(svc, now_ms, &out) &&
           out.action == NB_WAKE_ACTION_SESSION_ARMED;
}

static bool begin_listening(nb_wake_service_t *svc, uint32_t now_ms)
{
    nb_wake_output_t out;
    return nb_wake_service_on_audio_frame(svc, now_ms, true,
                                          NB_WAKE_SERVICE_CHUNK_SAMPLES, &out) &&
           out.action == NB_WAKE_ACTION_LISTEN_BEGIN_WITH_AUDIO;
}

static void test_init_is_idle(void)
{
    nb_wake_service_t svc;

    nb_wake_service_init(&svc);
    CHECK(nb_wake_service_get_state(&svc) == NB_WAKE_STATE_IDLE);
    CHECK(svc.next_session_id == 1u);
}

/* VOICE.md V-1: toque é vínculo afetivo; nunca abre sessão de voz. */
static void test_touch_never_opens_session(void)
{
    nb_wake_service_t svc;
    nb_wake_output_t out;

    nb_wake_service_init(&svc);
    make_ready(&svc);
    CHECK(!nb_wake_service_on_touch(&svc, &out));
    CHECK(nb_wake_service_get_state(&svc) == NB_WAKE_STATE_IDLE);
}

/* VOICE.md V-5: sem VAD real, o listening não cai silenciosamente pra
 * heurística. */
static void test_wake_without_vad_emits_honest_feedback(void)
{
    nb_wake_service_t svc;
    nb_wake_output_t out;

    nb_wake_service_init(&svc);
    nb_wake_service_set_routes(&svc, true, false);
    CHECK(nb_wake_service_on_wake(&svc, 100u, &out));
    CHECK(out.action == NB_WAKE_ACTION_FEEDBACK);
    CHECK(out.feedback == NB_WAKE_FEEDBACK_VAD_UNAVAILABLE);
    CHECK(nb_wake_service_get_state(&svc) == NB_WAKE_STATE_IDLE);
}

/* VOICE.md V-6: sem mente nem intent local, encerra com feedback honesto. */
static void test_wake_without_route_emits_honest_feedback(void)
{
    nb_wake_service_t svc;
    nb_wake_output_t out;

    nb_wake_service_init(&svc);
    nb_wake_service_set_vad_available(&svc, true);
    CHECK(nb_wake_service_on_wake(&svc, 100u, &out));
    CHECK(out.action == NB_WAKE_ACTION_FEEDBACK);
    CHECK(out.feedback == NB_WAKE_FEEDBACK_NO_ROUTE);
    CHECK(nb_wake_service_get_state(&svc) == NB_WAKE_STATE_IDLE);
}

/* VOICE.md V-3: diagnóstico/heurística fora de sessão não publica voz. */
static void test_audio_outside_session_never_publishes(void)
{
    nb_wake_service_t svc;
    nb_wake_output_t out;

    nb_wake_service_init(&svc);
    make_ready(&svc);
    CHECK(!nb_wake_service_on_audio_frame(&svc, 100u, true,
                                          NB_WAKE_SERVICE_CHUNK_SAMPLES, &out));
    CHECK(nb_wake_service_get_state(&svc) == NB_WAKE_STATE_IDLE);
}

/* VOICE.md V-2: LISTEN_START só ocorre dentro de sessão aberta por wake. */
static void test_first_voiced_chunk_after_wake_starts_listening(void)
{
    nb_wake_service_t svc;
    nb_wake_output_t out;

    nb_wake_service_init(&svc);
    make_ready(&svc);
    CHECK(wake_and_arm(&svc, 100u));
    CHECK(nb_wake_service_on_audio_frame(&svc, 120u, true,
                                         NB_WAKE_SERVICE_CHUNK_SAMPLES, &out));
    CHECK(out.action == NB_WAKE_ACTION_LISTEN_BEGIN_WITH_AUDIO);
    CHECK(out.session_id == 1u);
    CHECK(out.samples == NB_WAKE_SERVICE_CHUNK_SAMPLES);
    CHECK(nb_wake_service_get_state(&svc) == NB_WAKE_STATE_LISTENING);
}

static void test_additional_voiced_chunks_emit_audio_only(void)
{
    nb_wake_service_t svc;
    nb_wake_output_t out;

    nb_wake_service_init(&svc);
    make_ready(&svc);
    CHECK(wake_and_arm(&svc, 100u));
    CHECK(begin_listening(&svc, 120u));
    CHECK(nb_wake_service_on_audio_frame(&svc, 140u, true,
                                         NB_WAKE_SERVICE_CHUNK_SAMPLES, &out));
    CHECK(out.action == NB_WAKE_ACTION_LISTEN_AUDIO);
    CHECK(out.samples == NB_WAKE_SERVICE_CHUNK_SAMPLES);
}

/* VOICE.md V-4: LISTEN_END nunca sai órfão; sem start+audio, não encerra. */
static void test_armed_session_without_audio_never_emits_end(void)
{
    nb_wake_service_t svc;
    nb_wake_output_t out;

    nb_wake_service_init(&svc);
    make_ready(&svc);
    CHECK(wake_and_arm(&svc, 100u));
    CHECK(!nb_wake_service_tick(&svc, 5000u, &out));
    CHECK(nb_wake_service_get_state(&svc) == NB_WAKE_STATE_ARMED);
}

static void test_silence_after_audio_emits_listen_end(void)
{
    nb_wake_service_t svc;
    nb_wake_output_t out;

    nb_wake_service_init(&svc);
    make_ready(&svc);
    CHECK(wake_and_arm(&svc, 100u));
    CHECK(begin_listening(&svc, 120u));
    CHECK(nb_wake_service_tick(&svc, 120u + NB_WAKE_SERVICE_SILENCE_END_MS - 1u, &out) == false);
    CHECK(nb_wake_service_tick(&svc, 120u + NB_WAKE_SERVICE_SILENCE_END_MS, &out));
    CHECK(out.action == NB_WAKE_ACTION_LISTEN_END);
    CHECK(out.end_reason == NB_WAKE_END_REASON_SILENCE);
    CHECK(nb_wake_service_get_state(&svc) == NB_WAKE_STATE_IDLE);
}

static void test_max_duration_emits_listen_end(void)
{
    nb_wake_service_t svc;
    nb_wake_output_t out;

    nb_wake_service_init(&svc);
    make_ready(&svc);
    CHECK(wake_and_arm(&svc, 100u));
    CHECK(begin_listening(&svc, 120u));
    CHECK(nb_wake_service_tick(&svc, 120u + NB_WAKE_SERVICE_MAX_UTTERANCE_MS, &out));
    CHECK(out.action == NB_WAKE_ACTION_LISTEN_END);
    CHECK(out.end_reason == NB_WAKE_END_REASON_MAX_DURATION);
}

static void test_route_loss_mid_session_becomes_honest_feedback(void)
{
    nb_wake_service_t svc;
    nb_wake_output_t out;

    nb_wake_service_init(&svc);
    make_ready(&svc);
    CHECK(wake_and_arm(&svc, 100u));
    nb_wake_service_set_routes(&svc, false, false);
    CHECK(nb_wake_service_tick(&svc, 150u, &out));
    CHECK(out.action == NB_WAKE_ACTION_FEEDBACK);
    CHECK(out.feedback == NB_WAKE_FEEDBACK_NO_ROUTE);
    CHECK(nb_wake_service_get_state(&svc) == NB_WAKE_STATE_IDLE);
}

static void test_null_is_safe(void)
{
    nb_wake_output_t out;

    nb_wake_service_init(NULL);
    nb_wake_service_set_vad_available(NULL, true);
    nb_wake_service_set_routes(NULL, true, true);
    CHECK(!nb_wake_service_on_touch(NULL, &out));
    CHECK(!nb_wake_service_on_wake(NULL, 0u, &out));
    CHECK(!nb_wake_service_on_audio_frame(NULL, 0u, true, 1u, &out));
    CHECK(!nb_wake_service_tick(NULL, 0u, &out));
    CHECK(nb_wake_service_get_state(NULL) == NB_WAKE_STATE_IDLE);
}

int main(void)
{
    test_init_is_idle();
    test_touch_never_opens_session();
    test_wake_without_vad_emits_honest_feedback();
    test_wake_without_route_emits_honest_feedback();
    test_audio_outside_session_never_publishes();
    test_first_voiced_chunk_after_wake_starts_listening();
    test_additional_voiced_chunks_emit_audio_only();
    test_armed_session_without_audio_never_emits_end();
    test_silence_after_audio_emits_listen_end();
    test_max_duration_emits_listen_end();
    test_route_loss_mid_session_becomes_honest_feedback();
    test_null_is_safe();

    if (failures == 0) {
        printf("wake_service host_test: ok\n");
        return 0;
    }

    printf("wake_service host_test: %d failure(s)\n", failures);
    return 1;
}
