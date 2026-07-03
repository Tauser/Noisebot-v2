#include "../mind_link.h"

#include <stdio.h>

static int failures;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);           \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static void test_starts_idle(void)
{
    nb_mind_link_session_t session;

    nb_mind_link_session_init(&session);
    CHECK(nb_mind_link_session_get_state(&session) == NB_MIND_LINK_STATE_IDLE);
    CHECK(nb_mind_link_session_get_reconnect_attempts(&session) == 0u);
}

static void test_connected_moves_to_awaiting_hello_ack(void)
{
    nb_mind_link_session_t session;

    nb_mind_link_session_init(&session);
    nb_mind_link_session_on_connected(&session, 100u, 42u);
    CHECK(nb_mind_link_session_get_state(&session) == NB_MIND_LINK_STATE_AWAITING_HELLO_ACK);
}

static void test_hello_ack_matching_boot_id_reaches_ready(void)
{
    nb_mind_link_session_t session;

    nb_mind_link_session_init(&session);
    nb_mind_link_session_on_connected(&session, 100u, 42u);
    CHECK(nb_mind_link_session_on_hello_ack(&session, 150u, 42u));
    CHECK(nb_mind_link_session_get_state(&session) == NB_MIND_LINK_STATE_READY);
}

static void test_hello_ack_mismatched_boot_id_rejected(void)
{
    nb_mind_link_session_t session;

    nb_mind_link_session_init(&session);
    nb_mind_link_session_on_connected(&session, 100u, 42u);
    CHECK(!nb_mind_link_session_on_hello_ack(&session, 150u, 99u));
    CHECK(nb_mind_link_session_get_state(&session) == NB_MIND_LINK_STATE_DEAD);
}

static void test_hello_ack_ignored_outside_awaiting_state(void)
{
    nb_mind_link_session_t session;

    nb_mind_link_session_init(&session);
    CHECK(!nb_mind_link_session_on_hello_ack(&session, 100u, 42u));
    CHECK(nb_mind_link_session_get_state(&session) == NB_MIND_LINK_STATE_IDLE);
}

static void test_tick_signals_heartbeat_at_interval(void)
{
    nb_mind_link_session_t session;

    nb_mind_link_session_init(&session);
    nb_mind_link_session_on_connected(&session, 0u, 1u);
    nb_mind_link_session_on_hello_ack(&session, 0u, 1u);

    CHECK(!nb_mind_link_session_tick(&session, 500u));
    CHECK(nb_mind_link_session_tick(&session, 1000u));
    CHECK(!nb_mind_link_session_tick(&session, 1500u));
    CHECK(nb_mind_link_session_tick(&session, 2000u));
}

static void test_missed_heartbeats_kill_session(void)
{
    nb_mind_link_session_t session;

    nb_mind_link_session_init(&session);
    nb_mind_link_session_on_connected(&session, 0u, 1u);
    nb_mind_link_session_on_hello_ack(&session, 0u, 1u);

    /* servidor nunca manda heartbeat de volta; 3 intervalos = morte */
    nb_mind_link_session_tick(&session, 1000u);
    nb_mind_link_session_tick(&session, 2000u);
    CHECK(nb_mind_link_session_get_state(&session) == NB_MIND_LINK_STATE_READY);
    nb_mind_link_session_tick(&session, 3000u);
    CHECK(nb_mind_link_session_get_state(&session) == NB_MIND_LINK_STATE_DEAD);
    CHECK(nb_mind_link_session_get_reconnect_attempts(&session) == 1u);
}

static void test_heartbeat_received_resets_missed_window(void)
{
    nb_mind_link_session_t session;

    nb_mind_link_session_init(&session);
    nb_mind_link_session_on_connected(&session, 0u, 1u);
    nb_mind_link_session_on_hello_ack(&session, 0u, 1u);

    nb_mind_link_session_tick(&session, 1000u);
    nb_mind_link_session_tick(&session, 2000u);
    nb_mind_link_session_on_heartbeat_received(&session, 2500u);
    nb_mind_link_session_tick(&session, 3000u);
    CHECK(nb_mind_link_session_get_state(&session) == NB_MIND_LINK_STATE_READY);
}

static void test_disconnected_kills_session_and_counts_attempt(void)
{
    nb_mind_link_session_t session;

    nb_mind_link_session_init(&session);
    nb_mind_link_session_on_connected(&session, 0u, 1u);
    nb_mind_link_session_on_hello_ack(&session, 0u, 1u);

    nb_mind_link_session_on_disconnected(&session);
    CHECK(nb_mind_link_session_get_state(&session) == NB_MIND_LINK_STATE_DEAD);
    CHECK(nb_mind_link_session_get_reconnect_attempts(&session) == 1u);
}

static void test_successful_hello_ack_resets_reconnect_attempts(void)
{
    nb_mind_link_session_t session;

    nb_mind_link_session_init(&session);
    nb_mind_link_session_on_connected(&session, 0u, 1u);
    nb_mind_link_session_on_disconnected(&session);
    nb_mind_link_session_on_disconnected(&session);
    CHECK(nb_mind_link_session_get_reconnect_attempts(&session) == 2u);

    nb_mind_link_session_on_connected(&session, 100u, 1u);
    CHECK(nb_mind_link_session_on_hello_ack(&session, 150u, 1u));
    CHECK(nb_mind_link_session_get_reconnect_attempts(&session) == 0u);
}

static void test_backoff_delay_exponential_with_cap(void)
{
    CHECK(nb_mind_link_backoff_delay_ms(0u) == 500u);
    CHECK(nb_mind_link_backoff_delay_ms(1u) == 1000u);
    CHECK(nb_mind_link_backoff_delay_ms(2u) == 2000u);
    CHECK(nb_mind_link_backoff_delay_ms(3u) == 4000u);
    CHECK(nb_mind_link_backoff_delay_ms(10u) == 30000u);
    CHECK(nb_mind_link_backoff_delay_ms(1000u) == 30000u);
}

int main(void)
{
    test_starts_idle();
    test_connected_moves_to_awaiting_hello_ack();
    test_hello_ack_matching_boot_id_reaches_ready();
    test_hello_ack_mismatched_boot_id_rejected();
    test_hello_ack_ignored_outside_awaiting_state();
    test_tick_signals_heartbeat_at_interval();
    test_missed_heartbeats_kill_session();
    test_heartbeat_received_resets_missed_window();
    test_disconnected_kills_session_and_counts_attempt();
    test_successful_hello_ack_resets_reconnect_attempts();
    test_backoff_delay_exponential_with_cap();

    if (failures == 0) {
        printf("mind_link_session host_test: ok\n");
        return 0;
    }

    printf("mind_link_session host_test: %d failure(s)\n", failures);
    return 1;
}
