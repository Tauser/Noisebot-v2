#include "ota.h"

void nb_ota_init(nb_ota_t *ota, uint32_t running_secure_version)
{
    if (ota == 0) {
        return;
    }

    ota->running_secure_version = running_secure_version;
    ota->pending_secure_version = 0u;
    ota->committed_secure_version = running_secure_version;
    ota->pending_verify = false;
    ota->boot_healthy = false;
}

nb_ota_status_t nb_ota_accept_candidate(const nb_ota_t *ota,
                                        uint32_t candidate_secure_version)
{
    if (ota == 0) {
        return NB_OTA_STATUS_INVALID_ARG;
    }
    if (candidate_secure_version < ota->running_secure_version) {
        return NB_OTA_STATUS_DOWNGRADE_REJECTED;
    }
    if (candidate_secure_version > NB_OTA_MAX_SECURE_VERSION) {
        return NB_OTA_STATUS_INVALID_ARG;
    }

    return NB_OTA_STATUS_OK;
}

nb_ota_status_t nb_ota_set_pending(nb_ota_t *ota,
                                   uint32_t pending_secure_version)
{
    nb_ota_status_t status = nb_ota_accept_candidate(ota, pending_secure_version);
    if (status != NB_OTA_STATUS_OK) {
        return status;
    }

    ota->pending_secure_version = pending_secure_version;
    ota->pending_verify = true;
    ota->boot_healthy = false;
    return NB_OTA_STATUS_OK;
}

nb_ota_status_t nb_ota_mark_boot_healthy(nb_ota_t *ota)
{
    if (ota == 0) {
        return NB_OTA_STATUS_INVALID_ARG;
    }
    if (!ota->pending_verify) {
        return NB_OTA_STATUS_NO_PENDING_IMAGE;
    }

    ota->boot_healthy = true;
    return NB_OTA_STATUS_OK;
}

nb_ota_status_t nb_ota_commit_pending(nb_ota_t *ota)
{
    if (ota == 0) {
        return NB_OTA_STATUS_INVALID_ARG;
    }
    if (!ota->pending_verify) {
        return NB_OTA_STATUS_NO_PENDING_IMAGE;
    }
    if (!ota->boot_healthy) {
        return NB_OTA_STATUS_BOOT_NOT_HEALTHY;
    }
    if (ota->committed_secure_version == ota->pending_secure_version) {
        ota->pending_verify = false;
        return NB_OTA_STATUS_ALREADY_COMMITTED;
    }

    ota->running_secure_version = ota->pending_secure_version;
    ota->committed_secure_version = ota->pending_secure_version;
    ota->pending_verify = false;
    return NB_OTA_STATUS_OK;
}
