#ifndef NB_OTA_H
#define NB_OTA_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Largura do campo de secure_version na eFuse (CONFIG_BOOTLOADER_APP_SEC_VER_SIZE_EFUSE_FIELD).
 * O maior valor representável num campo desse tamanho é (2^bits)-1, não o
 * número de bits em si — usar NB_OTA_MAX_SECURE_VERSION_BITS diretamente
 * como teto rejeitaria quase toda a faixa válida (9..255). */
#define NB_OTA_MAX_SECURE_VERSION_BITS 8u
#define NB_OTA_MAX_SECURE_VERSION ((1u << NB_OTA_MAX_SECURE_VERSION_BITS) - 1u)

typedef enum {
    NB_OTA_STATUS_OK = 0,
    NB_OTA_STATUS_INVALID_ARG,
    NB_OTA_STATUS_DOWNGRADE_REJECTED,
    NB_OTA_STATUS_NO_PENDING_IMAGE,
    NB_OTA_STATUS_BOOT_NOT_HEALTHY,
    NB_OTA_STATUS_ALREADY_COMMITTED,
} nb_ota_status_t;

typedef struct {
    uint32_t running_secure_version;
    uint32_t pending_secure_version;
    uint32_t committed_secure_version;
    bool pending_verify;
    bool boot_healthy;
} nb_ota_t;

void nb_ota_init(nb_ota_t *ota, uint32_t running_secure_version);

nb_ota_status_t nb_ota_accept_candidate(const nb_ota_t *ota,
                                        uint32_t candidate_secure_version);

nb_ota_status_t nb_ota_set_pending(nb_ota_t *ota,
                                   uint32_t pending_secure_version);

nb_ota_status_t nb_ota_mark_boot_healthy(nb_ota_t *ota);

nb_ota_status_t nb_ota_commit_pending(nb_ota_t *ota);

#ifdef __cplusplus
}
#endif

#endif
