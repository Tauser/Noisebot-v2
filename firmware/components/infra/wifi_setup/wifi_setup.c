#include "wifi_setup.h"

#include <string.h>

void nb_wifi_setup_init(nb_wifi_setup_t *setup)
{
    if (setup == NULL) {
        return;
    }

    setup->state = NB_WIFI_SETUP_STATE_NOT_PROVISIONED;
}

nb_wifi_setup_state_t nb_wifi_setup_get_state(const nb_wifi_setup_t *setup)
{
    if (setup == NULL) {
        return NB_WIFI_SETUP_STATE_NOT_PROVISIONED;
    }

    return setup->state;
}

bool nb_wifi_setup_validate_ssid(const char *ssid)
{
    size_t len;

    if (ssid == NULL) {
        return false;
    }

    len = strlen(ssid);
    return len >= 1u && len <= NB_WIFI_SETUP_SSID_MAX_LEN;
}

bool nb_wifi_setup_validate_password(const char *password)
{
    size_t len;

    if (password == NULL) {
        return false;
    }

    len = strlen(password);
    if (len == 0u) {
        return true;
    }
    if (len < NB_WIFI_SETUP_PASSWORD_MIN_LEN || len > NB_WIFI_SETUP_PASSWORD_MAX_LEN) {
        return false;
    }

    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)password[i];
        if (c < 0x20u || c > 0x7eu) {
            return false;
        }
    }

    return true;
}

bool nb_wifi_setup_begin(nb_wifi_setup_t *setup)
{
    if (setup == NULL) {
        return false;
    }
    if (setup->state != NB_WIFI_SETUP_STATE_NOT_PROVISIONED &&
        setup->state != NB_WIFI_SETUP_STATE_FAILED) {
        return false;
    }

    setup->state = NB_WIFI_SETUP_STATE_PROVISIONING;
    return true;
}

bool nb_wifi_setup_complete(nb_wifi_setup_t *setup, const char *ssid,
                           const char *password)
{
    if (setup == NULL || setup->state != NB_WIFI_SETUP_STATE_PROVISIONING) {
        return false;
    }

    if (!nb_wifi_setup_validate_ssid(ssid) || !nb_wifi_setup_validate_password(password)) {
        setup->state = NB_WIFI_SETUP_STATE_FAILED;
        return false;
    }

    setup->state = NB_WIFI_SETUP_STATE_PROVISIONED;
    return true;
}

bool nb_wifi_setup_fail(nb_wifi_setup_t *setup)
{
    if (setup == NULL || setup->state != NB_WIFI_SETUP_STATE_PROVISIONING) {
        return false;
    }

    setup->state = NB_WIFI_SETUP_STATE_FAILED;
    return true;
}

void nb_wifi_setup_reset(nb_wifi_setup_t *setup)
{
    if (setup == NULL) {
        return;
    }

    setup->state = NB_WIFI_SETUP_STATE_NOT_PROVISIONED;
}
