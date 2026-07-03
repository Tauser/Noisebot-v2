#ifndef NB_MIND_LINK_SHELL_H
#define NB_MIND_LINK_SHELL_H

#include "esp_err.h"
#include "mind_link.h"

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

#ifdef __cplusplus
}
#endif

#endif
