#include "app_config.h"

#include <stddef.h>

static const nb_config_descriptor_t s_descriptors[NB_CONFIG_KEY_COUNT] = {
    [NB_CONFIG_KEY_LOG_MIN_LEVEL] = {
        .key = NB_CONFIG_KEY_LOG_MIN_LEVEL,
        .name = "log_min_level",
        .type = NB_CONFIG_TYPE_U32,
        .default_value = { .u32 = 1u }, /* NB_LOG_LEVEL_INFO */
        .min_value = { .u32 = 0u },      /* NB_LOG_LEVEL_DEBUG */
        .max_value = { .u32 = 4u },      /* NB_LOG_LEVEL_FAULT */
    },
    [NB_CONFIG_KEY_BOOT_FAIL_STREAK] = {
        .key = NB_CONFIG_KEY_BOOT_FAIL_STREAK,
        .name = "boot_fail_streak",
        .type = NB_CONFIG_TYPE_U32,
        .default_value = { .u32 = 0u },
        .min_value = { .u32 = 0u },
        .max_value = { .u32 = 255u },
    },
};

void nb_config_init(nb_config_t *config)
{
    if (config == NULL) {
        return;
    }

    for (size_t i = 0; i < NB_CONFIG_KEY_COUNT; ++i) {
        config->values[i].u32 = 0u;
        config->is_set[i] = false;
    }
}

const nb_config_descriptor_t *nb_config_get_descriptor(nb_config_key_t key)
{
    if (key >= NB_CONFIG_KEY_COUNT) {
        return NULL;
    }

    return &s_descriptors[key];
}

bool nb_config_get_u32(const nb_config_t *config, nb_config_key_t key, uint32_t *out_value)
{
    const nb_config_descriptor_t *descriptor = nb_config_get_descriptor(key);

    if (config == NULL || out_value == NULL || descriptor == NULL) {
        return false;
    }
    if (descriptor->type != NB_CONFIG_TYPE_U32) {
        return false;
    }

    *out_value = config->is_set[key] ? config->values[key].u32 : descriptor->default_value.u32;
    return true;
}

bool nb_config_set_u32(nb_config_t *config, nb_config_key_t key, uint32_t value)
{
    const nb_config_descriptor_t *descriptor = nb_config_get_descriptor(key);

    if (config == NULL || descriptor == NULL) {
        return false;
    }
    if (descriptor->type != NB_CONFIG_TYPE_U32) {
        return false;
    }
    if (value < descriptor->min_value.u32 || value > descriptor->max_value.u32) {
        return false;
    }

    config->values[key].u32 = value;
    config->is_set[key] = true;
    return true;
}

bool nb_config_get_i32(const nb_config_t *config, nb_config_key_t key, int32_t *out_value)
{
    const nb_config_descriptor_t *descriptor = nb_config_get_descriptor(key);

    if (config == NULL || out_value == NULL || descriptor == NULL) {
        return false;
    }
    if (descriptor->type != NB_CONFIG_TYPE_I32) {
        return false;
    }

    *out_value = config->is_set[key] ? config->values[key].i32 : descriptor->default_value.i32;
    return true;
}

bool nb_config_set_i32(nb_config_t *config, nb_config_key_t key, int32_t value)
{
    const nb_config_descriptor_t *descriptor = nb_config_get_descriptor(key);

    if (config == NULL || descriptor == NULL) {
        return false;
    }
    if (descriptor->type != NB_CONFIG_TYPE_I32) {
        return false;
    }
    if (value < descriptor->min_value.i32 || value > descriptor->max_value.i32) {
        return false;
    }

    config->values[key].i32 = value;
    config->is_set[key] = true;
    return true;
}
