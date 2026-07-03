#include "../wifi_setup.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);           \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static void test_ssid_validation(void)
{
    char long_ssid[NB_WIFI_SETUP_SSID_MAX_LEN + 2];

    CHECK(!nb_wifi_setup_validate_ssid(NULL));
    CHECK(!nb_wifi_setup_validate_ssid(""));
    CHECK(nb_wifi_setup_validate_ssid("a"));

    memset(long_ssid, 'a', sizeof(long_ssid) - 1u);
    long_ssid[sizeof(long_ssid) - 1u] = '\0';
    CHECK(strlen(long_ssid) == NB_WIFI_SETUP_SSID_MAX_LEN + 1u);
    CHECK(!nb_wifi_setup_validate_ssid(long_ssid));

    long_ssid[NB_WIFI_SETUP_SSID_MAX_LEN] = '\0';
    CHECK(strlen(long_ssid) == NB_WIFI_SETUP_SSID_MAX_LEN);
    CHECK(nb_wifi_setup_validate_ssid(long_ssid));
}

static void test_password_validation(void)
{
    CHECK(!nb_wifi_setup_validate_password(NULL));
    CHECK(nb_wifi_setup_validate_password(""));
    CHECK(!nb_wifi_setup_validate_password("short12"));
    CHECK(nb_wifi_setup_validate_password("12345678"));
    CHECK(!nb_wifi_setup_validate_password("has\tcontrol"));
}

static void test_begin_complete_happy_path(void)
{
    nb_wifi_setup_t setup;

    nb_wifi_setup_init(&setup);
    CHECK(nb_wifi_setup_get_state(&setup) == NB_WIFI_SETUP_STATE_NOT_PROVISIONED);

    CHECK(nb_wifi_setup_begin(&setup));
    CHECK(nb_wifi_setup_get_state(&setup) == NB_WIFI_SETUP_STATE_PROVISIONING);

    CHECK(nb_wifi_setup_complete(&setup, "minha-rede", "senha1234"));
    CHECK(nb_wifi_setup_get_state(&setup) == NB_WIFI_SETUP_STATE_PROVISIONED);
}

static void test_complete_with_invalid_input_fails(void)
{
    nb_wifi_setup_t setup;

    nb_wifi_setup_init(&setup);
    nb_wifi_setup_begin(&setup);

    CHECK(!nb_wifi_setup_complete(&setup, "rede", "curta"));
    CHECK(nb_wifi_setup_get_state(&setup) == NB_WIFI_SETUP_STATE_FAILED);
}

static void test_begin_rejected_while_already_active(void)
{
    nb_wifi_setup_t setup;

    nb_wifi_setup_init(&setup);
    nb_wifi_setup_begin(&setup);
    CHECK(!nb_wifi_setup_begin(&setup));

    nb_wifi_setup_complete(&setup, "rede", "");
    CHECK(!nb_wifi_setup_begin(&setup));
}

static void test_fail_only_valid_during_provisioning(void)
{
    nb_wifi_setup_t setup;

    nb_wifi_setup_init(&setup);
    CHECK(!nb_wifi_setup_fail(&setup));

    nb_wifi_setup_begin(&setup);
    CHECK(nb_wifi_setup_fail(&setup));
    CHECK(nb_wifi_setup_get_state(&setup) == NB_WIFI_SETUP_STATE_FAILED);

    CHECK(nb_wifi_setup_begin(&setup));
    CHECK(nb_wifi_setup_get_state(&setup) == NB_WIFI_SETUP_STATE_PROVISIONING);
}

static void test_reset_from_any_state(void)
{
    nb_wifi_setup_t setup;

    nb_wifi_setup_init(&setup);
    nb_wifi_setup_begin(&setup);
    nb_wifi_setup_complete(&setup, "rede", "senha1234");
    CHECK(nb_wifi_setup_get_state(&setup) == NB_WIFI_SETUP_STATE_PROVISIONED);

    nb_wifi_setup_reset(&setup);
    CHECK(nb_wifi_setup_get_state(&setup) == NB_WIFI_SETUP_STATE_NOT_PROVISIONED);
}

int main(void)
{
    test_ssid_validation();
    test_password_validation();
    test_begin_complete_happy_path();
    test_complete_with_invalid_input_fails();
    test_begin_rejected_while_already_active();
    test_fail_only_valid_during_provisioning();
    test_reset_from_any_state();

    if (failures == 0) {
        printf("wifi_setup host_test: ok\n");
        return 0;
    }

    printf("wifi_setup host_test: %d failure(s)\n", failures);
    return 1;
}
