#include "logger.h"

#include <string.h>

static void nb_logger_copy_truncate(char *dst, const char *src, size_t dst_size)
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

void nb_logger_init(nb_logger_t *logger, nb_log_level_t min_level)
{
    if (logger == NULL) {
        return;
    }

    memset(logger, 0, sizeof(*logger));
    logger->next_seq = 1u;
    logger->min_level = min_level;
}

void nb_logger_set_level(nb_logger_t *logger, nb_log_level_t min_level)
{
    if (logger == NULL) {
        return;
    }

    logger->min_level = min_level;
}

bool nb_logger_log(nb_logger_t *logger, uint32_t timestamp_ms,
                   nb_log_level_t level, const char *module,
                   const char *message)
{
    uint8_t idx;
    nb_log_entry_t *entry;

    if (logger == NULL || module == NULL || message == NULL) {
        return false;
    }

    if (level < logger->min_level) {
        ++logger->stats.filtered_total;
        return false;
    }

    if (logger->count < NB_LOGGER_CAPACITY) {
        idx = (uint8_t)((logger->head + logger->count) % NB_LOGGER_CAPACITY);
        ++logger->count;
    } else {
        idx = logger->head;
        logger->head = (uint8_t)((logger->head + 1u) % NB_LOGGER_CAPACITY);
        ++logger->stats.dropped_total;
    }

    entry = &logger->entries[idx];
    entry->seq = logger->next_seq++;
    entry->timestamp_ms = timestamp_ms;
    entry->level = level;
    nb_logger_copy_truncate(entry->module, module, NB_LOGGER_MODULE_MAX);
    nb_logger_copy_truncate(entry->message, message, NB_LOGGER_MESSAGE_MAX);

    ++logger->stats.logged_total;
    logger->stats.pending = logger->count;
    return true;
}

void nb_logger_get_stats(const nb_logger_t *logger, nb_logger_stats_t *out_stats)
{
    if (logger == NULL || out_stats == NULL) {
        return;
    }

    *out_stats = logger->stats;
}

size_t nb_logger_copy_all(const nb_logger_t *logger, nb_log_entry_t *out_entries,
                          size_t max_entries)
{
    size_t copied = 0;

    if (logger == NULL || out_entries == NULL || max_entries == 0u) {
        return 0u;
    }

    for (uint8_t i = 0; i < logger->count && copied < max_entries; ++i) {
        const uint8_t idx = (uint8_t)((logger->head + i) % NB_LOGGER_CAPACITY);
        out_entries[copied] = logger->entries[idx];
        ++copied;
    }

    return copied;
}

size_t nb_logger_drain_since(const nb_logger_t *logger, uint32_t *inout_last_seq,
                             nb_log_entry_t *out_entries, size_t max_entries,
                             bool *out_gap_detected)
{
    size_t copied = 0;
    uint32_t oldest_seq;

    if (out_gap_detected != NULL) {
        *out_gap_detected = false;
    }

    if (logger == NULL || inout_last_seq == NULL || out_entries == NULL) {
        return 0u;
    }

    if (logger->count == 0u) {
        return 0u;
    }

    oldest_seq = logger->entries[logger->head].seq;
    if (oldest_seq > *inout_last_seq + 1u) {
        if (out_gap_detected != NULL) {
            *out_gap_detected = true;
        }
        *inout_last_seq = oldest_seq - 1u;
    }

    for (uint8_t i = 0; i < logger->count && copied < max_entries; ++i) {
        const uint8_t idx = (uint8_t)((logger->head + i) % NB_LOGGER_CAPACITY);
        const nb_log_entry_t *entry = &logger->entries[idx];

        if (entry->seq <= *inout_last_seq) {
            continue;
        }

        out_entries[copied] = *entry;
        *inout_last_seq = entry->seq;
        ++copied;
    }

    return copied;
}
