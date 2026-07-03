#ifndef NB_LOGGER_H
#define NB_LOGGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro do logger estruturado (S1.3).
 *
 * Sem FreeRTOS, sem esp_*, sem malloc, sem leitura de clock/IO real: quem
 * chama passa o timestamp monotônico. Ring circular de tamanho fixo que
 * nunca bloqueia quem loga — cheio, sobrescreve a entrada mais antiga e
 * conta o drop.
 *
 * Duas formas de leitura, propositalmente distintas:
 *   - nb_logger_copy_all(): snapshot completo do que está no ring agora
 *     (uso: dump em shutdown/panic — nunca remove nada).
 *   - nb_logger_drain_since(): leitura incremental por cursor de sequência
 *     (uso futuro: worker de SD), com detecção de gap se o cursor ficou
 *     para trás do que já foi sobrescrito.
 *
 * A casca (mutex/task/worker SD/hook de panic) não existe ainda — ver
 * README.md deste componente e docs/ROADMAP.md §S1.3.
 */

#define NB_LOGGER_CAPACITY 128u
#define NB_LOGGER_MODULE_MAX 16u
#define NB_LOGGER_MESSAGE_MAX 96u

typedef enum {
    NB_LOG_LEVEL_DEBUG = 0,
    NB_LOG_LEVEL_INFO = 1,
    NB_LOG_LEVEL_WARN = 2,
    NB_LOG_LEVEL_ERROR = 3,
    NB_LOG_LEVEL_FAULT = 4,
} nb_log_level_t;

typedef struct {
    uint32_t seq;
    uint32_t timestamp_ms;
    nb_log_level_t level;
    char module[NB_LOGGER_MODULE_MAX];
    char message[NB_LOGGER_MESSAGE_MAX];
} nb_log_entry_t;

typedef struct {
    uint32_t logged_total;    /* entradas aceitas (passaram o filtro de nível) */
    uint32_t filtered_total;  /* entradas descartadas pelo filtro de nível */
    uint32_t dropped_total;   /* entradas sobrescritas antes de serem lidas */
    uint32_t pending;         /* entradas atualmente válidas no ring */
} nb_logger_stats_t;

typedef struct {
    nb_log_entry_t entries[NB_LOGGER_CAPACITY];
    uint8_t head;   /* índice da entrada mais antiga válida */
    uint8_t count;  /* quantidade de entradas válidas (<= capacidade) */
    uint32_t next_seq;
    nb_log_level_t min_level;
    nb_logger_stats_t stats;
} nb_logger_t;

/* Inicializa o logger; entradas com nível < min_level são descartadas antes
 * de consumir número de sequência. */
void nb_logger_init(nb_logger_t *logger, nb_log_level_t min_level);

void nb_logger_set_level(nb_logger_t *logger, nb_log_level_t min_level);

/* Registra uma entrada. message é copiada e truncada com segurança para
 * NB_LOGGER_MESSAGE_MAX-1 caracteres + NUL. module idem para
 * NB_LOGGER_MODULE_MAX-1. Retorna false se filtrada pelo nível ou se
 * argumentos obrigatórios forem NULL. */
bool nb_logger_log(nb_logger_t *logger, uint32_t timestamp_ms,
                   nb_log_level_t level, const char *module,
                   const char *message);

void nb_logger_get_stats(const nb_logger_t *logger, nb_logger_stats_t *out_stats);

/* Copia todas as entradas atualmente no ring, da mais antiga para a mais
 * nova, sem alterar estado. Uso: dump completo em shutdown/panic. */
size_t nb_logger_copy_all(const nb_logger_t *logger, nb_log_entry_t *out_entries,
                          size_t max_entries);

/*
 * Leitura incremental por cursor de sequência. *inout_last_seq é a
 * sequência da última entrada já consumida pelo chamador (0 se nunca
 * consumiu nada). Retorna as entradas com seq > *inout_last_seq, da mais
 * antiga para a mais nova, até max_entries, e avança *inout_last_seq para a
 * sequência da última entrada retornada.
 *
 * Se o ring já sobrescreveu entradas que o cursor ainda não tinha
 * consumido, out_gap_detected (se não-NULL) é marcado true e o cursor
 * salta para a entrada mais antiga disponível (nenhuma duplicação, perda
 * já contabilizada em stats.dropped_total).
 */
size_t nb_logger_drain_since(const nb_logger_t *logger, uint32_t *inout_last_seq,
                             nb_log_entry_t *out_entries, size_t max_entries,
                             bool *out_gap_detected);

#ifdef __cplusplus
}
#endif

#endif
