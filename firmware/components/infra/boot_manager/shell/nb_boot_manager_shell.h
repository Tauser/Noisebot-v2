#ifndef NB_BOOT_MANAGER_SHELL_H
#define NB_BOOT_MANAGER_SHELL_H

#include "boot_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Casca do boot_manager (S1.4): orquestra a inicialização real dos
 * componentes L1 que já têm casca pronta (logger, app_config), em
 * sequência, medindo a duração de cada fase com esp_timer.
 *
 * event_bus fica de fora por enquanto: o próprio README dele diz que a
 * casca só entra quando houver serviço publicando evento, e ela ainda não
 * existe — não é esquecimento, é escopo.
 *
 * Em SAFE_MODE, incrementa NB_CONFIG_KEY_BOOT_FAIL_STREAK (melhor esforço,
 * se o app_config tiver inicializado); em BOOT_OK, zera o contador.
 */
nb_boot_outcome_t nb_boot_manager_shell_run(nb_boot_report_t *out_report);

#ifdef __cplusplus
}
#endif

#endif
