#ifndef NB_MIND_LINK_SHELL_H
#define NB_MIND_LINK_SHELL_H

#include "esp_err.h"
#include "mind_link.h"
#include "nbp2.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Casca do mind_link (S1.7): task FreeRTOS que mantém a sessão NBP/2 via
 * TCP contra `CONFIG_NBP2_SERVER_HOST:CONFIG_NBP2_SERVER_PORT`, usando o
 * núcleo de sessão (`mind_link.h`) e o protocolo gerado (`nbp2.h`, via o
 * componente `nbp2`). Reconecta com backoff exponencial em falha/queda;
 * nunca bloqueia o boot nem trava o robô se o server estiver offline
 * (P1 — corpo funciona sem mente).
 */
esp_err_t nb_mind_link_shell_init(void);

nb_mind_link_state_t nb_mind_link_shell_get_state(void);

/* S3.5: pede o envio de EVENT_TIMER_FIRED (fire-and-forget -- sem sessão
 * READY, o reflexo local já disparou de qualquer jeito e o pedido é só
 * descartado, sem reenvio depois). Chamada de fora da task de mind_link
 * (main.c/schedule_core_shell), por isso não recebe o socket direto. */
void nb_mind_link_shell_notify_timer_fired(uint32_t timer_id);
bool nb_mind_link_shell_notify_event_wake(float score);
bool nb_mind_link_shell_notify_listen_start(uint32_t session_id, uint32_t sample_rate);
bool nb_mind_link_shell_notify_listen_audio(uint32_t session_id, const int16_t *pcm,
                                            uint32_t samples);
bool nb_mind_link_shell_notify_listen_end(uint32_t session_id);

/* S3.8, item 8: snapshot de STATUS pra enviar no próximo HEARTBEAT (não
 * têm sessão própria -- fire-and-forget, mesmo padrão de notify_timer_
 * fired; a task de mind_link reenvia o snapshot mais recente a cada
 * HEARTBEAT enquanto READY, sem fila/reenvio dedicado). Chamada de fora
 * da task de mind_link (main.c/face_demo_task), por isso não recebe o
 * socket direto -- mesma nota de notify_timer_fired. */
void nb_mind_link_shell_notify_status(const nbp2_msg_status_t *status);

#ifdef __cplusplus
}
#endif

#endif
