#ifndef NB_BOOT_MANAGER_H
#define NB_BOOT_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro do boot_manager (S1.4).
 *
 * Sem FreeRTOS, sem esp_*, sem malloc, sem leitura de clock real: quem
 * chama passa o timestamp de início/fim de cada fase. Sequência de fases
 * nomeadas com criticidade; decide BOOT_OK vs SAFE_MODE (qualquer fase
 * crítica que falhe) e monta um relatório determinístico (fase, sucesso,
 * duração, causa). Uma vez em SAFE_MODE, o motivo é pegajoso: falhas
 * seguintes não trocam o motivo registrado.
 *
 * A casca (orquestrar init real de logger/app_config/event_bus, medir
 * boot→idle) não existe ainda — ver README.md deste componente e
 * docs/ROADMAP.md §S1.4.
 */

#define NB_BOOT_MANAGER_MAX_PHASES 16u
#define NB_BOOT_PHASE_NAME_MAX 24u

typedef enum {
    NB_BOOT_PHASE_CRITICAL = 0,
    NB_BOOT_PHASE_NON_CRITICAL = 1,
} nb_boot_phase_criticality_t;

typedef enum {
    NB_BOOT_OUTCOME_PENDING = 0, /* nenhuma fase encerrada ainda */
    NB_BOOT_OUTCOME_OK = 1,
    NB_BOOT_OUTCOME_SAFE_MODE = 2,
} nb_boot_outcome_t;

typedef struct {
    char name[NB_BOOT_PHASE_NAME_MAX];
    nb_boot_phase_criticality_t criticality;
    bool success;
    uint32_t duration_ms;
} nb_boot_phase_record_t;

typedef struct {
    nb_boot_phase_record_t phases[NB_BOOT_MANAGER_MAX_PHASES];
    uint8_t phase_count;
    nb_boot_outcome_t outcome;
    char safe_mode_reason[NB_BOOT_PHASE_NAME_MAX];
    uint32_t total_duration_ms;
} nb_boot_report_t;

typedef struct {
    nb_boot_report_t report;
    bool phase_open;
    char current_name[NB_BOOT_PHASE_NAME_MAX];
    nb_boot_phase_criticality_t current_criticality;
    uint32_t current_start_ms;
} nb_boot_manager_t;

void nb_boot_manager_init(nb_boot_manager_t *manager);

/* Abre uma fase nova. Retorna false (sem alterar estado) se manager/name
 * forem NULL, se já houver uma fase aberta sem `end_phase` correspondente,
 * ou se a capacidade de fases do relatório já estiver esgotada. */
bool nb_boot_manager_begin_phase(nb_boot_manager_t *manager, const char *name,
                                 nb_boot_phase_criticality_t criticality,
                                 uint32_t start_ms);

/* Fecha a fase aberta, registra a duração (end_ms - start_ms) e atualiza o
 * outcome: fase crítica com success=false leva a SAFE_MODE, com o nome da
 * fase como motivo (pegajoso — não é sobrescrito por falhas seguintes).
 * Retorna false se não houver fase aberta. */
bool nb_boot_manager_end_phase(nb_boot_manager_t *manager, bool success, uint32_t end_ms);

void nb_boot_manager_get_report(const nb_boot_manager_t *manager, nb_boot_report_t *out_report);

#ifdef __cplusplus
}
#endif

#endif
