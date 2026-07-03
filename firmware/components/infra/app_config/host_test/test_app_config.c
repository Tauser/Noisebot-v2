#include "../app_config.h"

#include <stdio.h>

static int failures;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);           \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static void test_get_returns_default_when_never_set(void)
{
    nb_config_t config;
    uint32_t value;

    nb_config_init(&config);

    CHECK(nb_config_get_u32(&config, NB_CONFIG_KEY_LOG_MIN_LEVEL, &value));
    CHECK(value == 1u);
    CHECK(nb_config_get_u32(&config, NB_CONFIG_KEY_BOOT_FAIL_STREAK, &value));
    CHECK(value == 0u);
}

static void test_set_valid_value_is_read_back(void)
{
    nb_config_t config;
    uint32_t value;

    nb_config_init(&config);

    CHECK(nb_config_set_u32(&config, NB_CONFIG_KEY_LOG_MIN_LEVEL, 3u));
    CHECK(nb_config_get_u32(&config, NB_CONFIG_KEY_LOG_MIN_LEVEL, &value));
    CHECK(value == 3u);
}

static void test_set_out_of_range_is_rejected_and_keeps_previous(void)
{
    nb_config_t config;
    uint32_t value;

    nb_config_init(&config);

    CHECK(nb_config_set_u32(&config, NB_CONFIG_KEY_LOG_MIN_LEVEL, 2u));
    CHECK(!nb_config_set_u32(&config, NB_CONFIG_KEY_LOG_MIN_LEVEL, 5u));
    CHECK(!nb_config_set_u32(&config, NB_CONFIG_KEY_BOOT_FAIL_STREAK, 256u));

    CHECK(nb_config_get_u32(&config, NB_CONFIG_KEY_LOG_MIN_LEVEL, &value));
    CHECK(value == 2u);
}

static void test_wrong_type_accessor_is_rejected(void)
{
    nb_config_t config;
    int32_t signed_value;

    nb_config_init(&config);

    CHECK(!nb_config_set_i32(&config, NB_CONFIG_KEY_LOG_MIN_LEVEL, 2));
    CHECK(!nb_config_get_i32(&config, NB_CONFIG_KEY_LOG_MIN_LEVEL, &signed_value));
}

static void test_invalid_key_is_rejected(void)
{
    nb_config_t config;
    uint32_t value;

    nb_config_init(&config);

    CHECK(nb_config_get_descriptor((nb_config_key_t)NB_CONFIG_KEY_COUNT) == NULL);
    CHECK(!nb_config_get_u32(&config, (nb_config_key_t)NB_CONFIG_KEY_COUNT, &value));
    CHECK(!nb_config_set_u32(&config, (nb_config_key_t)NB_CONFIG_KEY_COUNT, 1u));
}

int main(void)
{
    test_get_returns_default_when_never_set();
    test_set_valid_value_is_read_back();
    test_set_out_of_range_is_rejected_and_keeps_previous();
    test_wrong_type_accessor_is_rejected();
    test_invalid_key_is_rejected();

    if (failures == 0) {
        printf("config host_test: ok\n");
        return 0;
    }

    printf("config host_test: %d failure(s)\n", failures);
    return 1;
}
