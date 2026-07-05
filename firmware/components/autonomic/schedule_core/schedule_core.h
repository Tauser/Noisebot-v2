#ifndef NB_SCHEDULE_CORE_H
#define NB_SCHEDULE_CORE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro do schedule_core (S3.5, camada L4, BEHAVIOR.md §5).
 * Timers/alarmes locais: disparo one-shot em `fire_at_unix_ms` absoluto --
 * sem recorrência (o protocolo NBP/2 `TIMER_SET`/`TIMER_CANCEL`/
 * `EVENT_TIMER_FIRED`, já com codegen, não define campo de recorrência).
 *
 * Array fixo de NB_SCHEDULE_MAX_TIMERS slots, sem malloc -- layout
 * compatível com blob de NVS (sem ponteiros, `nb_schedule_core_t` pode ser
 * persistido/restaurado por cópia de bytes direta pela casca).
 *
 * `create()` faz upsert por `timer_id`: id já existente atualiza o slot em
 * vez de duplicar -- suporta retry idempotente do protocolo (`idempotent:
 * true` em TIMER_SET) sem criar dois timers pro mesmo id. `tick()` varre
 * os slots contra `now_unix_ms` (injetado -- sem clock próprio, quem
 * chama é dono da hora, ex.: circadian_core) e libera cada slot vencido no
 * mesmo tick em que dispara -- nunca dispara duas vezes pro mesmo id.
 *
 * Sem FreeRTOS/ESP-IDF/NVS: tudo injetado pela casca.
 */

#define NB_SCHEDULE_MAX_TIMERS 8u
#define NB_SCHEDULE_LABEL_MAX_LEN 64u

typedef enum {
    NB_SCHEDULE_STATUS_OK = 0,
    NB_SCHEDULE_STATUS_ERR_FULL,
    NB_SCHEDULE_STATUS_ERR_NOT_FOUND,
    NB_SCHEDULE_STATUS_ERR_INVALID_ARG,
} nb_schedule_status_t;

typedef struct {
    bool in_use;
    uint32_t timer_id;
    uint64_t fire_at_unix_ms;
    char label[NB_SCHEDULE_LABEL_MAX_LEN + 1u];
} nb_schedule_timer_t;

typedef struct {
    nb_schedule_timer_t slots[NB_SCHEDULE_MAX_TIMERS];
} nb_schedule_core_t;

void nb_schedule_core_init(nb_schedule_core_t *core);

/* Cria ou atualiza (upsert por timer_id) um timer. label pode ser NULL
 * (rótulo vazio) -- truncado em NB_SCHEDULE_LABEL_MAX_LEN se maior.
 * ERR_FULL só quando timer_id é novo e não há slot livre. */
nb_schedule_status_t nb_schedule_core_create(nb_schedule_core_t *core, uint32_t timer_id,
                                             uint64_t fire_at_unix_ms, const char *label);

/* Libera o slot do timer_id. ERR_NOT_FOUND se não existia (idempotente na
 * prática -- cancelar de novo não é erro fatal pra quem chama). */
nb_schedule_status_t nb_schedule_core_cancel(nb_schedule_core_t *core, uint32_t timer_id);

/* Varre os slots contra now_unix_ms; cada slot vencido (fire_at_unix_ms
 * <= now_unix_ms) dispara: escreve o timer_id em out_fired_ids e libera o
 * slot, até max_fired disparos nesta chamada. Retorna quantos foram
 * escritos (<= max_fired). Slots vencidos além da capacidade do buffer
 * continuam in_use e disparam na próxima chamada -- nenhum é perdido,
 * nenhum dispara duas vezes. */
uint32_t nb_schedule_core_tick(nb_schedule_core_t *core, uint64_t now_unix_ms,
                               uint32_t *out_fired_ids, uint32_t max_fired);

bool nb_schedule_core_get(const nb_schedule_core_t *core, uint32_t timer_id,
                          nb_schedule_timer_t *out_timer);

uint32_t nb_schedule_core_count(const nb_schedule_core_t *core);

#ifdef __cplusplus
}
#endif

#endif
