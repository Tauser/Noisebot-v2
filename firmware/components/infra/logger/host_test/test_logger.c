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

static void test_log_and_copy_all_orders_oldest_to_newest(void)
{
    nb_logger_t logger;
    nb_log_entry_t out[8];
    size_t count;

    nb_logger_init(&logger, NB_LOG_LEVEL_DEBUG);

    CHECK(nb_logger_log(&logger, 100u, NB_LOG_LEVEL_INFO, "svc_a", "first"));
    CHECK(nb_logger_log(&logger, 200u, NB_LOG_LEVEL_WARN, "svc_b", "second"));
    CHECK(nb_logger_log(&logger, 300u, NB_LOG_LEVEL_ERROR, "svc_c", "third"));

    count = nb_logger_copy_all(&logger, out, 8);
    CHECK(count == 3u);
    CHECK(out[0].seq == 1u && strcmp(out[0].message, "first") == 0);
    CHECK(out[1].seq == 2u && strcmp(out[1].message, "second") == 0);
    CHECK(out[2].seq == 3u && strcmp(out[2].message, "third") == 0);
    CHECK(out[2].level == NB_LOG_LEVEL_ERROR);
    CHECK(out[2].timestamp_ms == 300u);
    CHECK(strcmp(out[2].module, "svc_c") == 0);
}

static void test_level_filter_drops_below_min_and_does_not_consume_seq(void)
{
    nb_logger_t logger;
    nb_logger_stats_t stats;
    nb_log_entry_t out[8];

    nb_logger_init(&logger, NB_LOG_LEVEL_WARN);

    CHECK(!nb_logger_log(&logger, 1u, NB_LOG_LEVEL_DEBUG, "svc", "ignored"));
    CHECK(!nb_logger_log(&logger, 2u, NB_LOG_LEVEL_INFO, "svc", "ignored"));
    CHECK(nb_logger_log(&logger, 3u, NB_LOG_LEVEL_WARN, "svc", "kept"));

    nb_logger_get_stats(&logger, &stats);
    CHECK(stats.filtered_total == 2u);
    CHECK(stats.logged_total == 1u);

    CHECK(nb_logger_copy_all(&logger, out, 8) == 1u);
    CHECK(out[0].seq == 1u); /* sequência só é consumida por entradas aceitas */
    CHECK(strcmp(out[0].message, "kept") == 0);
}

static void test_overflow_evicts_oldest_and_counts_drop(void)
{
    nb_logger_t logger;
    nb_logger_stats_t stats;
    nb_log_entry_t out[NB_LOGGER_CAPACITY];
    size_t count;
    char msg[16];

    nb_logger_init(&logger, NB_LOG_LEVEL_DEBUG);

    for (uint32_t i = 0; i < NB_LOGGER_CAPACITY + 5u; ++i) {
        snprintf(msg, sizeof(msg), "m%u", (unsigned)i);
        CHECK(nb_logger_log(&logger, i, NB_LOG_LEVEL_INFO, "svc", msg));
    }

    nb_logger_get_stats(&logger, &stats);
    CHECK(stats.dropped_total == 5u);
    CHECK(stats.pending == NB_LOGGER_CAPACITY);

    count = nb_logger_copy_all(&logger, out, NB_LOGGER_CAPACITY);
    CHECK(count == NB_LOGGER_CAPACITY);
    /* as 5 primeiras entradas (seq 1..5) foram sobrescritas; a mais antiga
     * remanescente deve ser a de seq 6. */
    CHECK(out[0].seq == 6u);
    CHECK(out[NB_LOGGER_CAPACITY - 1u].seq ==
          (uint32_t)NB_LOGGER_CAPACITY + 5u);
}

static void test_drain_since_incremental_two_calls(void)
{
    nb_logger_t logger;
    nb_log_entry_t out[8];
    uint32_t cursor = 0u;
    bool gap = true; /* valor sentinela pra provar que a função zera */
    size_t count;

    nb_logger_init(&logger, NB_LOG_LEVEL_DEBUG);

    CHECK(nb_logger_log(&logger, 10u, NB_LOG_LEVEL_INFO, "svc", "a"));
    CHECK(nb_logger_log(&logger, 20u, NB_LOG_LEVEL_INFO, "svc", "b"));

    count = nb_logger_drain_since(&logger, &cursor, out, 8, &gap);
    CHECK(count == 2u);
    CHECK(!gap);
    CHECK(cursor == 2u);
    CHECK(strcmp(out[0].message, "a") == 0);
    CHECK(strcmp(out[1].message, "b") == 0);

    /* nada de novo: segunda chamada não deve repetir nem quebrar. */
    count = nb_logger_drain_since(&logger, &cursor, out, 8, &gap);
    CHECK(count == 0u);
    CHECK(!gap);
    CHECK(cursor == 2u);

    CHECK(nb_logger_log(&logger, 30u, NB_LOG_LEVEL_INFO, "svc", "c"));
    count = nb_logger_drain_since(&logger, &cursor, out, 8, &gap);
    CHECK(count == 1u);
    CHECK(!gap);
    CHECK(cursor == 3u);
    CHECK(strcmp(out[0].message, "c") == 0);
}

static void test_drain_since_detects_gap_after_overwrite(void)
{
    nb_logger_t logger;
    nb_log_entry_t out[NB_LOGGER_CAPACITY];
    uint32_t cursor = 0u;
    bool gap = false;
    size_t count;
    char msg[16];

    nb_logger_init(&logger, NB_LOG_LEVEL_DEBUG);

    /* consumidor "lento": nunca chama drain_since até o ring já ter dado
     * uma volta inteira e meia. */
    for (uint32_t i = 0; i < NB_LOGGER_CAPACITY + 10u; ++i) {
        snprintf(msg, sizeof(msg), "m%u", (unsigned)i);
        CHECK(nb_logger_log(&logger, i, NB_LOG_LEVEL_INFO, "svc", msg));
    }

    count = nb_logger_drain_since(&logger, &cursor, out, NB_LOGGER_CAPACITY,
                                  &gap);
    CHECK(gap);
    CHECK(count == NB_LOGGER_CAPACITY);
    /* cursor deve pular direto para o começo do que ainda está disponível,
     * sem duplicar nem inventar entradas perdidas. */
    CHECK(out[0].seq == 11u);
    CHECK(cursor == (uint32_t)NB_LOGGER_CAPACITY + 10u);

    /* nova entrada depois do catch-up: sem gap, sequência contínua. */
    CHECK(nb_logger_log(&logger, 999u, NB_LOG_LEVEL_INFO, "svc", "fresh"));
    count = nb_logger_drain_since(&logger, &cursor, out, NB_LOGGER_CAPACITY,
                                  &gap);
    CHECK(!gap);
    CHECK(count == 1u);
    CHECK(strcmp(out[0].message, "fresh") == 0);
}

static void test_message_and_module_truncation_is_safe(void)
{
    nb_logger_t logger;
    nb_log_entry_t out[1];
    char long_message[256];
    char long_module[64];

    memset(long_message, 'x', sizeof(long_message) - 1u);
    long_message[sizeof(long_message) - 1u] = '\0';
    memset(long_module, 'y', sizeof(long_module) - 1u);
    long_module[sizeof(long_module) - 1u] = '\0';

    nb_logger_init(&logger, NB_LOG_LEVEL_DEBUG);
    CHECK(nb_logger_log(&logger, 1u, NB_LOG_LEVEL_INFO, long_module,
                        long_message));

    CHECK(nb_logger_copy_all(&logger, out, 1) == 1u);
    CHECK(strlen(out[0].message) == NB_LOGGER_MESSAGE_MAX - 1u);
    CHECK(strlen(out[0].module) == NB_LOGGER_MODULE_MAX - 1u);
    CHECK(out[0].message[NB_LOGGER_MESSAGE_MAX - 1u] == '\0');
}

static void test_invalid_args_are_rejected(void)
{
    nb_logger_t logger;

    nb_logger_init(&logger, NB_LOG_LEVEL_DEBUG);
    CHECK(!nb_logger_log(NULL, 0u, NB_LOG_LEVEL_INFO, "svc", "msg"));
    CHECK(!nb_logger_log(&logger, 0u, NB_LOG_LEVEL_INFO, NULL, "msg"));
    CHECK(!nb_logger_log(&logger, 0u, NB_LOG_LEVEL_INFO, "svc", NULL));
    CHECK(nb_logger_copy_all(NULL, NULL, 0) == 0u);
    CHECK(nb_logger_drain_since(&logger, NULL, NULL, 0, NULL) == 0u);
}

int main(void)
{
    test_log_and_copy_all_orders_oldest_to_newest();
    test_level_filter_drops_below_min_and_does_not_consume_seq();
    test_overflow_evicts_oldest_and_counts_drop();
    test_drain_since_incremental_two_calls();
    test_drain_since_detects_gap_after_overwrite();
    test_message_and_module_truncation_is_safe();
    test_invalid_args_are_rejected();

    if (failures != 0) {
        printf("logger host_test: %d failure(s)\n", failures);
        return 1;
    }

    printf("logger host_test: ok\n");
    return 0;
}
