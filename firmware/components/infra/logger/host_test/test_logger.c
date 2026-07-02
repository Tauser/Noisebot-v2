#include "../logger.h"

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

typedef struct {
    uint32_t count;
    uint32_t last_seq;
} sink_ctx_t;

static bool counting_sink(const nb_log_record_t *record, void *user_ctx)
{
    sink_ctx_t *ctx = (sink_ctx_t *)user_ctx;

    CHECK(record != NULL);
    CHECK(ctx != NULL);
    if (record != NULL && ctx != NULL) {
        ctx->last_seq = record->seq;
        ++ctx->count;
    }
    return true;
}

static void test_write_and_copy_recent_are_chronological(void)
{
    nb_logger_t logger;
    nb_log_record_t records[4];

    nb_logger_init(&logger, (nb_logger_config_t){.dev_build = false});

    CHECK(nb_logger_write(&logger, 10u, NB_LOG_LEVEL_INFO, "boot", "a",
                          false) == NB_LOGGER_OK);
    CHECK(nb_logger_write(&logger, 20u, NB_LOG_LEVEL_WARN, "boot", "b",
                          false) == NB_LOGGER_OK);

    CHECK(nb_logger_copy_recent(&logger, records, 4u) == 2u);
    CHECK(records[0].seq == 0u);
    CHECK(records[0].timestamp_ms == 10u);
    CHECK(strcmp(records[0].module, "boot") == 0);
    CHECK(strcmp(records[0].message, "a") == 0);
    CHECK(records[1].seq == 1u);
    CHECK(records[1].level == NB_LOG_LEVEL_WARN);
}

static void test_ring_wrap_overwrites_oldest(void)
{
    nb_logger_t logger;
    nb_log_record_t records[NB_LOGGER_CAPACITY];
    nb_logger_stats_t stats;

    nb_logger_init(&logger, (nb_logger_config_t){.dev_build = false});

    for (uint32_t i = 0; i < NB_LOGGER_CAPACITY + 3u; ++i) {
        CHECK(nb_logger_write(&logger, i, NB_LOG_LEVEL_INFO, "wrap", "entry",
                              false) == NB_LOGGER_OK);
    }

    CHECK(nb_logger_copy_recent(&logger, records, NB_LOGGER_CAPACITY) ==
          NB_LOGGER_CAPACITY);
    CHECK(records[0].seq == 3u);
    CHECK(records[NB_LOGGER_CAPACITY - 1u].seq ==
          NB_LOGGER_CAPACITY + 2u);

    nb_logger_get_stats(&logger, &stats);
    CHECK(stats.written == NB_LOGGER_CAPACITY + 3u);
    CHECK(stats.overwritten == 3u);
    CHECK(stats.pending == NB_LOGGER_CAPACITY);
}

static void test_sensitive_messages_are_redacted_in_prod(void)
{
    nb_logger_t logger;
    nb_log_record_t record;
    nb_logger_stats_t stats;

    nb_logger_init(&logger, (nb_logger_config_t){.dev_build = false});
    CHECK(nb_logger_write(&logger, 1u, NB_LOG_LEVEL_INFO, "wifi",
                          "token=abc", true) == NB_LOGGER_OK);

    CHECK(nb_logger_copy_recent(&logger, &record, 1u) == 1u);
    CHECK(record.redacted);
    CHECK(record.sensitive);
    CHECK(strcmp(record.message, "[redacted]") == 0);

    nb_logger_get_stats(&logger, &stats);
    CHECK(stats.redacted == 1u);
}

static void test_sensitive_messages_survive_in_dev(void)
{
    nb_logger_t logger;
    nb_log_record_t record;

    nb_logger_init(&logger, (nb_logger_config_t){.dev_build = true});
    CHECK(nb_logger_write(&logger, 1u, NB_LOG_LEVEL_DEBUG, "wifi",
                          "token=dev-only", true) == NB_LOGGER_OK);

    CHECK(nb_logger_copy_recent(&logger, &record, 1u) == 1u);
    CHECK(!record.redacted);
    CHECK(strcmp(record.message, "token=dev-only") == 0);
}

static void test_long_text_is_truncated(void)
{
    nb_logger_t logger;
    nb_log_record_t record;
    nb_logger_stats_t stats;
    char long_message[NB_LOGGER_MESSAGE_MAX + 8u];

    memset(long_message, 'x', sizeof(long_message));
    long_message[sizeof(long_message) - 1u] = '\0';

    nb_logger_init(&logger, (nb_logger_config_t){.dev_build = false});
    CHECK(nb_logger_write(&logger, 1u, NB_LOG_LEVEL_INFO,
                          "module-name-that-is-long", long_message, false) ==
          NB_LOGGER_OK);

    CHECK(nb_logger_copy_recent(&logger, &record, 1u) == 1u);
    CHECK(record.truncated);
    CHECK(record.message[NB_LOGGER_MESSAGE_MAX - 1u] == '\0');
    CHECK(record.module[NB_LOGGER_MODULE_MAX - 1u] == '\0');

    nb_logger_get_stats(&logger, &stats);
    CHECK(stats.truncated == 1u);
}

static void test_copy_since_and_dump_support_flush_worker(void)
{
    nb_logger_t logger;
    nb_log_record_t records[4];
    sink_ctx_t sink = {0};

    nb_logger_init(&logger, (nb_logger_config_t){.dev_build = false});

    for (uint32_t i = 0; i < 5u; ++i) {
        CHECK(nb_logger_write(&logger, i, NB_LOG_LEVEL_INFO, "flush",
                              "entry", false) == NB_LOGGER_OK);
    }

    CHECK(nb_logger_copy_since(&logger, 2u, records, 4u) == 3u);
    CHECK(records[0].seq == 2u);
    CHECK(records[2].seq == 4u);

    CHECK(nb_logger_dump_recent(&logger, counting_sink, &sink) == 5u);
    CHECK(sink.count == 5u);
    CHECK(sink.last_seq == 4u);
}

int main(void)
{
    test_write_and_copy_recent_are_chronological();
    test_ring_wrap_overwrites_oldest();
    test_sensitive_messages_are_redacted_in_prod();
    test_sensitive_messages_survive_in_dev();
    test_long_text_is_truncated();
    test_copy_since_and_dump_support_flush_worker();

    if (failures != 0) {
        printf("logger host_test: %d failure(s)\n", failures);
        return 1;
    }

    printf("logger host_test: ok\n");
    return 0;
}
