#ifndef NB_CIRCADIAN_CORE_SHELL_H
#define NB_CIRCADIAN_CORE_SHELL_H

#include "circadian_core.h"
#include "esp_err.h"
#include "tiny_fsm.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Casca do circadian_core (S3.4): dona da instância única do núcleo puro.
 * No init, carrega o último horário conhecido do NVS (via app_config,
 * fallback de BEHAVIOR.md §5) ou semeia um horário de bancada arbitrário
 * (~20h, início de DUSK) se nunca sincronizado -- documentado como só-pra-
 * demo, já que não há servidor real enviando TIME_SYNC ainda.
 *
 * Relógio de bancada acelerado (NB_CIRCADIAN_SHELL_BENCH_ACCEL_FACTOR): o
 * dt_ms recebido é multiplicado antes de entrar no núcleo, comprimindo um
 * dia inteiro em minutos -- só assim dá pra observar dormir/acordar de
 * verdade em uma sessão de bancada curta (mesmo padrão do pulso sintético
 * do emotion_core antes do toque real, S2.5->S3.1). Remover/recalibrar
 * quando TIME_SYNC real existir.
 */

void nb_circadian_core_shell_init(void);

/* unix_ms estimado agora (S3.5: fonte de hora do schedule_core). */
uint64_t nb_circadian_core_shell_now_unix_ms(void);

/* Recalibra a âncora com um TIME_SYNC real. Chamado por reflex_engine_shell
 * (mesma camada L4, único leitor do event_bus -- FIFO de leitor único, não
 * dá pra ter dois consumidores fazendo poll do mesmo bus) quando drena um
 * NB_EVENT_TYPE_TIME_SYNC publicado por mind_link_shell (L3, cross-layer
 * via bus, ARCHITECTURE.md §2). */
void nb_circadian_core_shell_apply_time_sync(uint64_t unix_ms);

/* Avança dt_ms (acelerado internamente). Aplica SLEEP/WAKE_HOUR em *fsm
 * nas bordas de entrada em NIGHT/DAWN (com
 * retry até tiny_fsm confirmar -- nunca briga se a máquina estiver ocupada
 * em outro estado), persiste o horário estimado periodicamente no NVS.
 * Retorna a saída resolvida pro chamador aplicar em idle_engine/led_service. */
nb_circadian_output_t nb_circadian_core_shell_tick(uint32_t dt_ms, nb_tiny_fsm_t *fsm);

#ifdef __cplusplus
}
#endif

#endif
