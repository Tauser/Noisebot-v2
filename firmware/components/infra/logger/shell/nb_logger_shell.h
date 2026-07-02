#ifndef NB_LOGGER_SHELL_H
#define NB_LOGGER_SHELL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "logger.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Casca FreeRTOS do logger (S1.3, fatia parcial).
 *
 * Serializa acesso ao núcleo `nb_logger_t` (singleton) com um mutex, para uso
 * seguro por múltiplas tasks, e injeta timestamp real via esp_timer. Não
 * inclui task dedicada, worker de SD nem hook de dump em panic — essa fatia
 * cobre só a serialização; o resto continua bloqueado por S0.3 e por
 * validação em bancada do coredump (ver docs/ROADMAP.md §S1.3 e README.md
 * deste componente).
 */

esp_err_t nb_logger_shell_init(nb_log_level_t min_level);

bool nb_logger_shell_log(nb_log_level_t level, const char *module, const char *message);

void nb_logger_shell_set_level(nb_log_level_t min_level);

void nb_logger_shell_get_stats(nb_logger_stats_t *out_stats);

size_t nb_logger_shell_copy_all(nb_log_entry_t *out_entries, size_t max_entries);

size_t nb_logger_shell_drain_since(uint32_t *inout_last_seq, nb_log_entry_t *out_entries,
                                   size_t max_entries, bool *out_gap_detected);

#ifdef __cplusplus
}
#endif

#endif
