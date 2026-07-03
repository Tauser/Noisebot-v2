#include "ota.h"

#include <assert.h>

static void test_accepts_same_or_newer_secure_version(void)
{
    nb_ota_t ota;

    nb_ota_init(&ota, 3u);
    assert(nb_ota_accept_candidate(&ota, 3u) == NB_OTA_STATUS_OK);
    assert(nb_ota_accept_candidate(&ota, 4u) == NB_OTA_STATUS_OK);
}

static void test_rejects_downgrade_and_out_of_range_version(void)
{
    nb_ota_t ota;

    nb_ota_init(&ota, 3u);
    assert(nb_ota_accept_candidate(&ota, 2u) ==
           NB_OTA_STATUS_DOWNGRADE_REJECTED);
    /* Campo de 8 bits na eFuse: toda a faixa 3..255 é válida, só acima
     * disso (256+) é que não cabe no campo. */
    assert(nb_ota_accept_candidate(&ota, NB_OTA_MAX_SECURE_VERSION) ==
           NB_OTA_STATUS_OK);
    assert(nb_ota_accept_candidate(&ota, NB_OTA_MAX_SECURE_VERSION + 1u) ==
           NB_OTA_STATUS_INVALID_ARG);
}

static void test_pending_image_requires_healthy_boot_before_commit(void)
{
    nb_ota_t ota;

    nb_ota_init(&ota, 1u);
    assert(nb_ota_set_pending(&ota, 2u) == NB_OTA_STATUS_OK);
    assert(ota.pending_verify);
    assert(nb_ota_commit_pending(&ota) == NB_OTA_STATUS_BOOT_NOT_HEALTHY);

    assert(nb_ota_mark_boot_healthy(&ota) == NB_OTA_STATUS_OK);
    assert(nb_ota_commit_pending(&ota) == NB_OTA_STATUS_OK);
    assert(!ota.pending_verify);
    assert(ota.committed_secure_version == 2u);
}

static void test_commit_without_pending_image_is_rejected(void)
{
    nb_ota_t ota;

    nb_ota_init(&ota, 1u);
    assert(nb_ota_mark_boot_healthy(&ota) == NB_OTA_STATUS_NO_PENDING_IMAGE);
    assert(nb_ota_commit_pending(&ota) == NB_OTA_STATUS_NO_PENDING_IMAGE);
}

static void test_null_arguments_are_rejected(void)
{
    assert(nb_ota_accept_candidate(0, 1u) == NB_OTA_STATUS_INVALID_ARG);
    assert(nb_ota_set_pending(0, 1u) == NB_OTA_STATUS_INVALID_ARG);
    assert(nb_ota_mark_boot_healthy(0) == NB_OTA_STATUS_INVALID_ARG);
    assert(nb_ota_commit_pending(0) == NB_OTA_STATUS_INVALID_ARG);
}

int main(void)
{
    test_accepts_same_or_newer_secure_version();
    test_rejects_downgrade_and_out_of_range_version();
    test_pending_image_requires_healthy_boot_before_commit();
    test_commit_without_pending_image_is_rejected();
    test_null_arguments_are_rejected();
    return 0;
}
