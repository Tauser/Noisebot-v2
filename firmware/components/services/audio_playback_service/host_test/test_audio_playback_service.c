#include "../audio_playback_service.h"

#include <stdio.h>

static int failures;

#define CHECK(expr)                                                        \
    do {                                                                   \
        if (!(expr)) {                                                     \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);         \
            ++failures;                                                    \
        }                                                                  \
    } while (0)

static void test_begin_audio_end_drains_in_order(void)
{
    nb_audio_playback_service_t svc;
    int16_t ring[8];
    int16_t out[8];
    const int16_t chunk_a[] = {10, 11, 12};
    const int16_t chunk_b[] = {20, 21};

    CHECK(nb_audio_playback_service_init(&svc, ring, 8u, 4u));
    CHECK(nb_audio_playback_service_on_say_begin(&svc, 7u, 16000u));
    CHECK(nb_audio_playback_service_on_say_audio(&svc, 7u, chunk_a, 3u) == 3u);
    CHECK(nb_audio_playback_service_on_say_audio(&svc, 7u, chunk_b, 2u) == 2u);
    CHECK(nb_audio_playback_service_on_say_end(&svc, 7u));
    CHECK(nb_audio_playback_service_get_state(&svc) == NB_AUDIO_PLAYBACK_STATE_DRAINING);
    CHECK(nb_audio_playback_service_consume(&svc, out, 8u) == 5u);
    CHECK(out[0] == 10);
    CHECK(out[1] == 11);
    CHECK(out[2] == 12);
    CHECK(out[3] == 20);
    CHECK(out[4] == 21);
    CHECK(nb_audio_playback_service_get_state(&svc) == NB_AUDIO_PLAYBACK_STATE_IDLE);
}

static void test_overflow_drops_newest_and_preserves_prefix(void)
{
    nb_audio_playback_service_t svc;
    int16_t ring[4];
    int16_t out[4];
    const int16_t chunk[] = {1, 2, 3, 4, 5, 6};

    CHECK(nb_audio_playback_service_init(&svc, ring, 4u, 4u));
    CHECK(nb_audio_playback_service_on_say_begin(&svc, 9u, 16000u));
    CHECK(nb_audio_playback_service_on_say_audio(&svc, 9u, chunk, 6u) == 4u);
    CHECK(svc.dropped_samples == 2u);
    CHECK(nb_audio_playback_service_get_buffered_samples(&svc) == 4u);
    CHECK(nb_audio_playback_service_consume(&svc, out, 4u) == 4u);
    CHECK(out[0] == 1);
    CHECK(out[1] == 2);
    CHECK(out[2] == 3);
    CHECK(out[3] == 4);
}

static void test_cancel_discards_buffer_and_generates_fade(void)
{
    nb_audio_playback_service_t svc;
    int16_t ring[8];
    int16_t drain[4];
    int16_t fade[4];
    const int16_t chunk[] = {1000, 800, 600, 400};

    CHECK(nb_audio_playback_service_init(&svc, ring, 8u, 4u));
    CHECK(nb_audio_playback_service_on_say_begin(&svc, 3u, 16000u));
    CHECK(nb_audio_playback_service_on_say_audio(&svc, 3u, chunk, 4u) == 4u);
    CHECK(nb_audio_playback_service_consume(&svc, drain, 2u) == 2u);
    CHECK(drain[0] == 1000);
    CHECK(drain[1] == 800);
    CHECK(nb_audio_playback_service_on_say_cancel(&svc, 3u));
    CHECK(nb_audio_playback_service_get_state(&svc) == NB_AUDIO_PLAYBACK_STATE_FADING);
    CHECK(nb_audio_playback_service_get_buffered_samples(&svc) == 0u);
    CHECK(nb_audio_playback_service_get_stop_reason(&svc) == NB_AUDIO_PLAYBACK_STOP_CANCELLED);
    CHECK(nb_audio_playback_service_consume(&svc, fade, 4u) == 4u);
    CHECK(fade[0] == 800);
    CHECK(fade[1] == 600);
    CHECK(fade[2] == 400);
    CHECK(fade[3] == 200);
    CHECK(nb_audio_playback_service_get_state(&svc) == NB_AUDIO_PLAYBACK_STATE_IDLE);
}

static void test_server_drop_without_last_sample_ends_immediately(void)
{
    nb_audio_playback_service_t svc;
    int16_t ring[8];

    CHECK(nb_audio_playback_service_init(&svc, ring, 8u, 4u));
    CHECK(nb_audio_playback_service_on_say_begin(&svc, 5u, 16000u));
    CHECK(nb_audio_playback_service_on_say_audio(&svc, 5u, ring, 0u) == 0u);
    CHECK(nb_audio_playback_service_on_server_drop(&svc));
    CHECK(nb_audio_playback_service_get_state(&svc) == NB_AUDIO_PLAYBACK_STATE_IDLE);
}

static void test_server_drop_drains_buffer_then_fades(void)
{
    nb_audio_playback_service_t svc;
    int16_t ring[8];
    int16_t out[8];
    const int16_t chunk[] = {1000, 800, 600, 400};

    CHECK(nb_audio_playback_service_init(&svc, ring, 8u, 4u));
    CHECK(nb_audio_playback_service_on_say_begin(&svc, 6u, 16000u));
    CHECK(nb_audio_playback_service_on_say_audio(&svc, 6u, chunk, 4u) == 4u);
    CHECK(nb_audio_playback_service_on_server_drop(&svc));
    CHECK(nb_audio_playback_service_get_state(&svc) == NB_AUDIO_PLAYBACK_STATE_DRAINING);
    CHECK(nb_audio_playback_service_get_buffered_samples(&svc) == 4u);
    CHECK(nb_audio_playback_service_get_stop_reason(&svc) ==
          NB_AUDIO_PLAYBACK_STOP_SERVER_DROPPED);
    CHECK(nb_audio_playback_service_consume(&svc, out, 8u) == 8u);
    CHECK(out[0] == 1000);
    CHECK(out[1] == 800);
    CHECK(out[2] == 600);
    CHECK(out[3] == 400);
    CHECK(out[4] == 400);
    CHECK(out[5] == 300);
    CHECK(out[6] == 200);
    CHECK(out[7] == 100);
    CHECK(nb_audio_playback_service_get_state(&svc) == NB_AUDIO_PLAYBACK_STATE_IDLE);
}

static void test_new_begin_replaces_stale_turn(void)
{
    nb_audio_playback_service_t svc;
    int16_t ring[8];
    int16_t out[4];
    const int16_t old_chunk[] = {1, 2, 3};
    const int16_t new_chunk[] = {9, 8};

    CHECK(nb_audio_playback_service_init(&svc, ring, 8u, 4u));
    CHECK(nb_audio_playback_service_on_say_begin(&svc, 1u, 16000u));
    CHECK(nb_audio_playback_service_on_say_audio(&svc, 1u, old_chunk, 3u) == 3u);
    CHECK(nb_audio_playback_service_on_say_begin(&svc, 2u, 16000u));
    CHECK(nb_audio_playback_service_on_say_audio(&svc, 1u, old_chunk, 3u) == 0u);
    CHECK(nb_audio_playback_service_on_say_audio(&svc, 2u, new_chunk, 2u) == 2u);
    CHECK(nb_audio_playback_service_on_say_end(&svc, 2u));
    CHECK(nb_audio_playback_service_consume(&svc, out, 4u) == 2u);
    CHECK(out[0] == 9);
    CHECK(out[1] == 8);
}

static void test_null_and_mismatched_turn_are_safe(void)
{
    nb_audio_playback_service_t svc;
    int16_t ring[4];
    int16_t out[2];
    const int16_t chunk[] = {1, 2};

    CHECK(!nb_audio_playback_service_init(NULL, ring, 4u, 4u));
    CHECK(!nb_audio_playback_service_init(&svc, NULL, 4u, 4u));
    CHECK(nb_audio_playback_service_init(&svc, ring, 4u, 4u));
    CHECK(!nb_audio_playback_service_on_say_begin(&svc, 0u, 16000u));
    CHECK(nb_audio_playback_service_on_say_begin(&svc, 7u, 16000u));
    CHECK(nb_audio_playback_service_on_say_audio(&svc, 99u, chunk, 2u) == 0u);
    CHECK(!nb_audio_playback_service_on_say_end(&svc, 99u));
    CHECK(!nb_audio_playback_service_on_say_cancel(&svc, 99u));
    CHECK(nb_audio_playback_service_consume(NULL, out, 2u) == 0u);
    CHECK(nb_audio_playback_service_consume(&svc, NULL, 2u) == 0u);
}

int main(void)
{
    test_begin_audio_end_drains_in_order();
    test_overflow_drops_newest_and_preserves_prefix();
    test_cancel_discards_buffer_and_generates_fade();
    test_server_drop_without_last_sample_ends_immediately();
    test_server_drop_drains_buffer_then_fades();
    test_new_begin_replaces_stale_turn();
    test_null_and_mismatched_turn_are_safe();

    if (failures == 0) {
        printf("audio_playback_service host_test: ok\n");
        return 0;
    }

    printf("audio_playback_service host_test: %d failure(s)\n", failures);
    return 1;
}
