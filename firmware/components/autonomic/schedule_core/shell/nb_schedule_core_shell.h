#ifndef NB_SCHEDULE_CORE_SHELL_H
#define NB_SCHEDULE_CORE_SHELL_H

#include "schedule_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Casca do schedule_core (S3.5): dona da instância única do núcleo puro,
 * persistida em NVS (namespace próprio "nb_sched", blob único -- mesmo
 * padrão de mind_link_token_shell, o modelo escalar do app_config não
 * serve pra um array de slots). Carrega o blob salvo no init; cobre
 * "reboot não perde nem duplica" (timer vencido enquanto desligado
 * dispara no primeiro tick, e sai liberado do próprio disparo).
 */

void nb_schedule_core_shell_init(void);

/* Chamado por reflex_engine_shell (único leitor do event_bus) quando
 * drena um NB_EVENT_TYPE_TIMER publicado por mind_link_shell -- mind_link
 * é L3, schedule_core é L4, "camada chama só pra baixo" proíbe chamada
 * direta L3->L4. */
void nb_schedule_core_shell_handle_set(uint32_t timer_id, uint64_t fire_at_unix_ms);
void nb_schedule_core_shell_handle_cancel(uint32_t timer_id);

/* Usa nb_circadian_core_shell_now_unix_ms() como fonte de hora; dispara os
 * timers vencidos, persiste o array em NVS quando algo muda, escreve os
 * ids disparados em out_fired_ids (até max_fired). Retorna quantos
 * dispararam nesta chamada. */
uint32_t nb_schedule_core_shell_tick(uint32_t *out_fired_ids, uint32_t max_fired);

#ifdef __cplusplus
}
#endif

#endif
