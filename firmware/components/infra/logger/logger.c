#include "logger.h"

#include <string.h>

static const char *const NB_LOGGER_REDACTED = "[redacted]";

static bool nb_logger_level_is_valid(nb_log_level_t level)
{
    return level == NB_LOG_LEVEL_DEBUG || level == NB_LOG_LEVEL_INFO ||
           level == NB_LOG_LEVEL_WARN || level == NB_LOG_LEVEL_ERROR ||
           level == NB_LOG_LEVEL_PANIC;
}

static void nb_logger_copy_text(char *dst,
                                size_t dst_size,
                                const char *src,
                                bool *out_truncated)
{
    size_t i = 0;

    if (dst_size == 0u) {
        return;
    }

    if (src == NULL) {
        src = "";
    }

    while (i + 1u < dst_size && src[i] != '\0') {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';

    if (out_truncated != NULL && src[i] != '\0') {
        *out_truncated = true;
    }
}

static uint8_t nb_logger_oldest_index(const nb_logger_t *logger)
{
    if (logger->count < NB_LOGGER_CAPACITY) {
        return 0u;
    }
    return logger->head;
}

static uint8_t nb_logger_record_index(const nb_logger_t *logger,
                                      uint8_t offset)
{
    return (uint8_t)((nb_logger_oldest_index(logger) + offset) %
                     NB_LOGGER_CAPACITY);
}

void nb_logger_init(nb_logger_t *logger, nb_logger_config_t config)
{
    if (logger == NULL) {
        return;
    }

    memset(logger, 0, sizeof(*logger));
    logger->config = config;
}

nb_logger_status_t nb_logger_write(nb_logger_t *logger,
                                   uint32_t timestamp_ms,
                                   nb_log_level_t level,
                                   const char *module,
                                   const char *message,
                                   bool sensitive)
{
    nb_log_record_t *record = NULL;
    bool truncated = false;
    bool redacted = false;

    if (logger == NULL || module == NULL || message == NULL ||
        !nb_logger_level_is_valid(level)) {
        return NB_LOGGER_ERR_INVALID_ARG;
    }

    if (logger->count == NB_LOGGER_CAPACITY) {
        ++logger->overwritten;
    } else {
        ++logger->count;
    }

    record = &logger->records[logger->head];
    memset(record, 0, sizeof(*record));

    record->seq = logger->next_seq;
    record->timestamp_ms = timestamp_ms;
    record->level = level;
    record->sensitive = sensitive;

    nb_logger_copy_text(record->module, sizeof(record->module), module,
                        &truncated);

    if (sensitive && !logger->config.dev_build) {
        nb_logger_copy_text(record->message, sizeof(record->message),
                            NB_LOGGER_REDACTED, &truncated);
        redacted = true;
    } else {
        nb_logger_copy_text(record->message, sizeof(record->message), message,
                            &truncated);
    }

    record->truncated = truncated;
    record->redacted = redacted;

    if (truncated) {
        ++logger->truncated;
    }
    if (redacted) {
        ++logger->redacted;
    }

    ++logger->next_seq;
    logger->head = (uint8_t)((logger->head + 1u) % NB_LOGGER_CAPACITY);

    return NB_LOGGER_OK;
}

size_t nb_logger_copy_recent(const nb_logger_t *logger,
                             nb_log_record_t *out_records,
                             size_t max_records)
{
    size_t copied = 0;

    if (logger == NULL || out_records == NULL || max_records == 0u) {
        return 0u;
    }

    while (copied < logger->count && copied < max_records) {
        out_records[copied] =
            logger->records[nb_logger_record_index(logger, (uint8_t)copied)];
        ++copied;
    }

    return copied;
}

size_t nb_logger_copy_since(const nb_logger_t *logger,
                            uint32_t first_seq,
                            nb_log_record_t *out_records,
                            size_t max_records)
{
    size_t copied = 0;

    if (logger == NULL || out_records == NULL || max_records == 0u) {
        return 0u;
    }

    for (uint8_t i = 0; i < logger->count && copied < max_records; ++i) {
        const nb_log_record_t *record =
            &logger->records[nb_logger_record_index(logger, i)];
        if (record->seq >= first_seq) {
            out_records[copied] = *record;
            ++copied;
        }
    }

    return copied;
}

size_t nb_logger_dump_recent(const nb_logger_t *logger,
                             nb_logger_record_sink_t sink,
                             void *user_ctx)
{
    size_t emitted = 0;

    if (logger == NULL || sink == NULL) {
        return 0u;
    }

    for (uint8_t i = 0; i < logger->count; ++i) {
        const nb_log_record_t *record =
            &logger->records[nb_logger_record_index(logger, i)];
        if (!sink(record, user_ctx)) {
            break;
        }
        ++emitted;
    }

    return emitted;
}

void nb_logger_get_stats(const nb_logger_t *logger,
                         nb_logger_stats_t *out_stats)
{
    if (logger == NULL || out_stats == NULL) {
        return;
    }

    out_stats->written = logger->next_seq;
    out_stats->overwritten = logger->overwritten;
    out_stats->redacted = logger->redacted;
    out_stats->truncated = logger->truncated;
    out_stats->next_seq = logger->next_seq;
    out_stats->pending = logger->count;
}
