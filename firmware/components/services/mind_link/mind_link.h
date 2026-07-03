#ifndef NB_MIND_LINK_H
#define NB_MIND_LINK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro da sessão do mind_link (S1.7 — PROTOCOL.md §3).
 *
 * Sem FreeRTOS, sem sockets, sem ESP-IDF: máquina de estados da sessão
 * NBP/2 (handshake HELLO/HELLO_ACK, heartbeat, detecção de sessão morta) e
 * cálculo de backoff de reconexão. Clock injetado (timestamps em ms,
 * passados pelo chamador). A casca (socket TCP real, reconexão de verdade)
 * fica para depois — ver README.md deste componente e docs/ROADMAP.md
 * §S1.7.
 */

#define NB_MIND_LINK_HEARTBEAT_INTERVAL_MS 1000u
#define NB_MIND_LINK_MAX_MISSED_HEARTBEATS 3u
#define NB_MIND_LINK_BACKOFF_BASE_MS 500u
#define NB_MIND_LINK_BACKOFF_MAX_MS 30000u

typedef enum {
    NB_MIND_LINK_STATE_IDLE = 0,             /* sem transporte conectado */
    NB_MIND_LINK_STATE_AWAITING_HELLO_ACK = 1, /* HELLO enviado, aguardando ACK */
    NB_MIND_LINK_STATE_READY = 2,             /* sessão estabelecida */
    NB_MIND_LINK_STATE_DEAD = 3,              /* sessão morta (timeout/erro) */
} nb_mind_link_state_t;

typedef struct {
    nb_mind_link_state_t state;
    uint32_t boot_id;
    uint32_t last_heartbeat_sent_at_ms;
    uint32_t last_heartbeat_received_at_ms;
    uint32_t reconnect_attempts;
} nb_mind_link_session_t;

void nb_mind_link_session_init(nb_mind_link_session_t *session);

nb_mind_link_state_t nb_mind_link_session_get_state(const nb_mind_link_session_t *session);

uint32_t nb_mind_link_session_get_reconnect_attempts(const nb_mind_link_session_t *session);

/* Transporte conectou: registra boot_id do HELLO que a casca vai enviar e
 * entra em AWAITING_HELLO_ACK, de qualquer estado anterior. */
void nb_mind_link_session_on_connected(nb_mind_link_session_t *session, uint32_t now_ms,
                                       uint32_t boot_id);

/* HELLO_ACK recebido. Só produz efeito em AWAITING_HELLO_ACK. boot_id deve
 * bater com o enviado no HELLO (PROTOCOL.md §3.4) — caso contrário a sessão
 * vai para DEAD e a função retorna false. Em sucesso: READY, zera o
 * contador de tentativas de reconexão, baseline de heartbeat = now_ms. */
bool nb_mind_link_session_on_hello_ack(nb_mind_link_session_t *session, uint32_t now_ms,
                                       uint32_t boot_id);

/* Heartbeat do server. Só tem efeito em READY. */
void nb_mind_link_session_on_heartbeat_received(nb_mind_link_session_t *session, uint32_t now_ms);

/* Transporte caiu (erro de socket, EOF, etc). Vai para DEAD de qualquer
 * estado e incrementa o contador de tentativas de reconexão. */
void nb_mind_link_session_on_disconnected(nb_mind_link_session_t *session);

/* Chamado periodicamente pela casca. Em READY: se
 * NB_MIND_LINK_MAX_MISSED_HEARTBEATS intervalos se passaram sem heartbeat
 * do server, a sessão morre (DEAD) e retorna false. Senão, se já passou um
 * intervalo desde o último heartbeat enviado, marca como enviado agora e
 * retorna true (a casca deve mandar o HEARTBEAT). Fora de READY, sempre
 * retorna false. */
bool nb_mind_link_session_tick(nb_mind_link_session_t *session, uint32_t now_ms);

/* Backoff exponencial determinístico (sem jitter, decisão consciente para
 * manter testável): min(BASE * 2^attempt, MAX). */
uint32_t nb_mind_link_backoff_delay_ms(uint32_t attempt);

#ifdef __cplusplus
}
#endif

#endif
