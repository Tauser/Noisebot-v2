#include "boot_manager.h"

#include <string.h>

static void nb_boot_manager_copy_truncate(char *dst, const char *src, size_t dst_size)
{
    size_t i = 0;

    if (dst_size == 0u) {
        return;
    }

    for (; i < dst_size - 1u && src[i] != '\0'; ++i) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

void nb_boot_manager_init(nb_boot_manager_t *manager)
{
    if (manager == NULL) {
        return;
    }

    memset(manager, 0, sizeof(*manager));
    manager->report.outcome = NB_BOOT_OUTCOME_PENDING;
}

bool nb_boot_manager_begin_phase(nb_boot_manager_t *manager, const char *name,
                                 nb_boot_phase_criticality_t criticality,
                                 uint32_t start_ms)
{
    if (manager == NULL || name == NULL) {
        return false;
    }
    if (manager->phase_open) {
        return false;
    }
    if (manager->report.phase_count >= NB_BOOT_MANAGER_MAX_PHASES) {
        return false;
    }

    nb_boot_manager_copy_truncate(manager->current_name, name, NB_BOOT_PHASE_NAME_MAX);
    manager->current_criticality = criticality;
    manager->current_start_ms = start_ms;
    manager->phase_open = true;
    return true;
}

bool nb_boot_manager_end_phase(nb_boot_manager_t *manager, bool success, uint32_t end_ms)
{
    nb_boot_phase_record_t *record;

    if (manager == NULL || !manager->phase_open) {
        return false;
    }

    record = &manager->report.phases[manager->report.phase_count];
    nb_boot_manager_copy_truncate(record->name, manager->current_name, NB_BOOT_PHASE_NAME_MAX);
    record->criticality = manager->current_criticality;
    record->success = success;
    record->duration_ms = end_ms - manager->current_start_ms;

    manager->report.total_duration_ms += record->duration_ms;
    ++manager->report.phase_count;
    manager->phase_open = false;

    if (!success && manager->current_criticality == NB_BOOT_PHASE_CRITICAL) {
        if (manager->report.outcome != NB_BOOT_OUTCOME_SAFE_MODE) {
            manager->report.outcome = NB_BOOT_OUTCOME_SAFE_MODE;
            nb_boot_manager_copy_truncate(manager->report.safe_mode_reason, record->name,
                                          NB_BOOT_PHASE_NAME_MAX);
        }
    } else if (manager->report.outcome == NB_BOOT_OUTCOME_PENDING) {
        manager->report.outcome = NB_BOOT_OUTCOME_OK;
    }

    return true;
}

void nb_boot_manager_get_report(const nb_boot_manager_t *manager, nb_boot_report_t *out_report)
{
    if (manager == NULL || out_report == NULL) {
        return;
    }

    *out_report = manager->report;
}
