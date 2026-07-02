#ifndef NB_LOGGER_H
#define NB_LOGGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NB_LOGGER_CAPACITY 64u
#define NB_LOGGER_MODULE_MAX 16u
#define NB_LOGGER_MESSAGE_MAX 96u

typedef enum {
    NB_LOG_LEVEL_DEBUG = 0,
    NB_LOG_LEVEL_INFO = 1,
    NB_LOG_LEVEL_WARN = 2,
    NB_LOG_LEVEL_ERROR = 3,
    NB_LOG_LEVEL_PANIC = 4,
} nb_log_level_t;

typedef enum {
    NB_LOGGER_OK = 0,
    NB_LOGGER_ERR_INVALID_ARG = 1,
    NB_LOGGER_ERR_EMPTY = 2,
} nb_logger_status_t;

typedef struct {
    bool dev_build;
} nb_logger_config_t;

typedef struct {
    uint32_t seq;
    uint32_t timestamp_ms;
    nb_log_level_t level;
    char module[NB_LOGGER_MODULE_MAX];
    char message[NB_LOGGER_MESSAGE_MAX];
    bool truncated;
    bool redacted;
    bool sensitive;
} nb_log_record_t;

typedef struct {
    uint32_t written;
    uint32_t overwritten;
    uint32_t redacted;
    uint32_t truncated;
    uint32_t next_seq;
    uint8_t pending;
} nb_logger_stats_t;

typedef struct {
    nb_logger_config_t config;
    nb_log_record_t records[NB_LOGGER_CAPACITY];
    uint8_t head;
    uint8_t count;
    uint32_t next_seq;
    uint32_t overwritten;
    uint32_t redacted;
    uint32_t truncated;
} nb_logger_t;

typedef bool (*nb_logger_record_sink_t)(const nb_log_record_t *record,
                                        void *user_ctx);

void nb_logger_init(nb_logger_t *logger, nb_logger_config_t config);

nb_logger_status_t nb_logger_write(nb_logger_t *logger,
                                   uint32_t timestamp_ms,
                                   nb_log_level_t level,
                                   const char *module,
                                   const char *message,
                                   bool sensitive);

size_t nb_logger_copy_recent(const nb_logger_t *logger,
                             nb_log_record_t *out_records,
                             size_t max_records);

size_t nb_logger_copy_since(const nb_logger_t *logger,
                            uint32_t first_seq,
                            nb_log_record_t *out_records,
                            size_t max_records);

size_t nb_logger_dump_recent(const nb_logger_t *logger,
                             nb_logger_record_sink_t sink,
                             void *user_ctx);

void nb_logger_get_stats(const nb_logger_t *logger,
                         nb_logger_stats_t *out_stats);

#ifdef __cplusplus
}
#endif

#endif
