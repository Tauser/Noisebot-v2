#include "mind_link.h"

#include <stddef.h>

void nb_mind_link_session_init(nb_mind_link_session_t *session)
{
    if (session == NULL) {
        return;
    }

    session->state = NB_MIND_LINK_STATE_IDLE;
    session->boot_id = 0u;
    session->last_heartbeat_sent_at_ms = 0u;
    session->last_heartbeat_received_at_ms = 0u;
    session->reconnect_attempts = 0u;
}

nb_mind_link_state_t nb_mind_link_session_get_state(const nb_mind_link_session_t *session)
{
    if (session == NULL) {
        return NB_MIND_LINK_STATE_IDLE;
    }

    return session->state;
}

uint32_t nb_mind_link_session_get_reconnect_attempts(const nb_mind_link_session_t *session)
{
    if (session == NULL) {
        return 0u;
    }

    return session->reconnect_attempts;
}

void nb_mind_link_session_on_connected(nb_mind_link_session_t *session, uint32_t now_ms,
                                       uint32_t boot_id)
{
    if (session == NULL) {
        return;
    }

    session->state = NB_MIND_LINK_STATE_AWAITING_HELLO_ACK;
    session->boot_id = boot_id;
    session->last_heartbeat_sent_at_ms = now_ms;
    session->last_heartbeat_received_at_ms = now_ms;
}

bool nb_mind_link_session_on_hello_ack(nb_mind_link_session_t *session, uint32_t now_ms,
                                       uint32_t boot_id)
{
    if (session == NULL || session->state != NB_MIND_LINK_STATE_AWAITING_HELLO_ACK) {
        return false;
    }

    if (boot_id != session->boot_id) {
        session->state = NB_MIND_LINK_STATE_DEAD;
        return false;
    }

    session->state = NB_MIND_LINK_STATE_READY;
    session->reconnect_attempts = 0u;
    session->last_heartbeat_sent_at_ms = now_ms;
    session->last_heartbeat_received_at_ms = now_ms;
    return true;
}

void nb_mind_link_session_on_heartbeat_received(nb_mind_link_session_t *session, uint32_t now_ms)
{
    if (session == NULL || session->state != NB_MIND_LINK_STATE_READY) {
        return;
    }

    session->last_heartbeat_received_at_ms = now_ms;
}

void nb_mind_link_session_on_disconnected(nb_mind_link_session_t *session)
{
    if (session == NULL) {
        return;
    }

    session->state = NB_MIND_LINK_STATE_DEAD;
    ++session->reconnect_attempts;
}

bool nb_mind_link_session_tick(nb_mind_link_session_t *session, uint32_t now_ms)
{
    uint32_t dead_after_ms;

    if (session == NULL || session->state != NB_MIND_LINK_STATE_READY) {
        return false;
    }

    dead_after_ms = NB_MIND_LINK_HEARTBEAT_INTERVAL_MS * NB_MIND_LINK_MAX_MISSED_HEARTBEATS;
    if ((uint32_t)(now_ms - session->last_heartbeat_received_at_ms) >= dead_after_ms) {
        session->state = NB_MIND_LINK_STATE_DEAD;
        ++session->reconnect_attempts;
        return false;
    }

    if ((uint32_t)(now_ms - session->last_heartbeat_sent_at_ms) >= NB_MIND_LINK_HEARTBEAT_INTERVAL_MS) {
        session->last_heartbeat_sent_at_ms = now_ms;
        return true;
    }

    return false;
}

uint32_t nb_mind_link_backoff_delay_ms(uint32_t attempt)
{
    uint64_t delay = (uint64_t)NB_MIND_LINK_BACKOFF_BASE_MS;
    uint32_t i;

    for (i = 0u; i < attempt && delay < NB_MIND_LINK_BACKOFF_MAX_MS; ++i) {
        delay *= 2u;
    }
    if (delay > NB_MIND_LINK_BACKOFF_MAX_MS) {
        delay = NB_MIND_LINK_BACKOFF_MAX_MS;
    }

    return (uint32_t)delay;
}
