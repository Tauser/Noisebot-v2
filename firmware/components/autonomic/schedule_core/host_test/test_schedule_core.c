#include "../schedule_core.h"

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

static void test_create_and_get(void) {
    nb_schedule_core_t core;
    nb_schedule_timer_t out;

    nb_schedule_core_init(&core);
    CHECK(nb_schedule_core_create(&core, 1u, 5000u, "cafe") == NB_SCHEDULE_STATUS_OK);
    CHECK(nb_schedule_core_count(&core) == 1u);
    CHECK(nb_schedule_core_get(&core, 1u, &out));
    CHECK(out.fire_at_unix_ms == 5000u);
    CHECK(strcmp(out.label, "cafe") == 0);
}

static void test_create_null_label_is_empty(void) {
    nb_schedule_core_t core;
    nb_schedule_timer_t out;

    nb_schedule_core_init(&core);
    CHECK(nb_schedule_core_create(&core, 1u, 1000u, NULL) == NB_SCHEDULE_STATUS_OK);
    CHECK(nb_schedule_core_get(&core, 1u, &out));
    CHECK(out.label[0] == '\0');
}

static void test_create_truncates_long_label(void) {
    nb_schedule_core_t core;
    nb_schedule_timer_t out;
    char long_label[200];

    memset(long_label, 'x', sizeof(long_label) - 1u);
    long_label[sizeof(long_label) - 1u] = '\0';

    nb_schedule_core_init(&core);
    CHECK(nb_schedule_core_create(&core, 1u, 1000u, long_label) == NB_SCHEDULE_STATUS_OK);
    CHECK(nb_schedule_core_get(&core, 1u, &out));
    CHECK(strlen(out.label) == NB_SCHEDULE_LABEL_MAX_LEN);
}

/* TIMER_SET e idempotent no protocolo -- criar de novo com o mesmo id
 * atualiza em vez de duplicar (retry idempotente nao ocupa um segundo
 * slot). */
static void test_create_same_id_upserts_not_duplicates(void) {
    nb_schedule_core_t core;
    nb_schedule_timer_t out;

    nb_schedule_core_init(&core);
    CHECK(nb_schedule_core_create(&core, 1u, 1000u, "a") == NB_SCHEDULE_STATUS_OK);
    CHECK(nb_schedule_core_create(&core, 1u, 2000u, "b") == NB_SCHEDULE_STATUS_OK);
    CHECK(nb_schedule_core_count(&core) == 1u);
    CHECK(nb_schedule_core_get(&core, 1u, &out));
    CHECK(out.fire_at_unix_ms == 2000u);
    CHECK(strcmp(out.label, "b") == 0);
}

static void test_full_when_all_slots_distinct_ids(void) {
    nb_schedule_core_t core;

    nb_schedule_core_init(&core);
    for (uint32_t i = 0; i < NB_SCHEDULE_MAX_TIMERS; ++i) {
        CHECK(nb_schedule_core_create(&core, i + 1u, 1000u, NULL) == NB_SCHEDULE_STATUS_OK);
    }
    CHECK(nb_schedule_core_create(&core, 999u, 1000u, NULL) == NB_SCHEDULE_STATUS_ERR_FULL);

    /* Cancelar um libera espaço pro próximo create. */
    CHECK(nb_schedule_core_cancel(&core, 1u) == NB_SCHEDULE_STATUS_OK);
    CHECK(nb_schedule_core_create(&core, 999u, 1000u, NULL) == NB_SCHEDULE_STATUS_OK);
}

static void test_cancel_not_found(void) {
    nb_schedule_core_t core;

    nb_schedule_core_init(&core);
    CHECK(nb_schedule_core_cancel(&core, 42u) == NB_SCHEDULE_STATUS_ERR_NOT_FOUND);
}

static void test_tick_fires_expired_and_clears_slot(void) {
    nb_schedule_core_t core;
    uint32_t fired[NB_SCHEDULE_MAX_TIMERS];

    nb_schedule_core_init(&core);
    nb_schedule_core_create(&core, 1u, 1000u, NULL);
    nb_schedule_core_create(&core, 2u, 5000u, NULL);

    uint32_t count = nb_schedule_core_tick(&core, 1000u, fired, NB_SCHEDULE_MAX_TIMERS);
    CHECK(count == 1u);
    CHECK(fired[0] == 1u);
    CHECK(nb_schedule_core_count(&core) == 1u); /* só o timer 2 continua */

    /* Não dispara de novo -- o slot já foi liberado no tick anterior. */
    count = nb_schedule_core_tick(&core, 999999u, fired, NB_SCHEDULE_MAX_TIMERS);
    CHECK(count == 1u);
    CHECK(fired[0] == 2u);
    CHECK(nb_schedule_core_count(&core) == 0u);
}

static void test_tick_respects_max_fired_and_retries_next_call(void) {
    nb_schedule_core_t core;
    uint32_t fired[NB_SCHEDULE_MAX_TIMERS];

    nb_schedule_core_init(&core);
    for (uint32_t i = 0; i < 3u; ++i) {
        nb_schedule_core_create(&core, i + 1u, 1000u, NULL);
    }

    uint32_t count = nb_schedule_core_tick(&core, 1000u, fired, 2u);
    CHECK(count == 2u);
    CHECK(nb_schedule_core_count(&core) == 1u); /* 1 ainda pendente */

    count = nb_schedule_core_tick(&core, 1000u, fired, 2u);
    CHECK(count == 1u);
    CHECK(nb_schedule_core_count(&core) == 0u);
}

static void test_tick_not_yet_due_stays_pending(void) {
    nb_schedule_core_t core;
    uint32_t fired[NB_SCHEDULE_MAX_TIMERS];

    nb_schedule_core_init(&core);
    nb_schedule_core_create(&core, 1u, 5000u, NULL);

    CHECK(nb_schedule_core_tick(&core, 4999u, fired, NB_SCHEDULE_MAX_TIMERS) == 0u);
    CHECK(nb_schedule_core_count(&core) == 1u);
}

static void test_null_is_safe(void) {
    nb_schedule_timer_t out;
    uint32_t fired[1];

    nb_schedule_core_init(NULL);
    CHECK(nb_schedule_core_create(NULL, 1u, 1000u, NULL) == NB_SCHEDULE_STATUS_ERR_INVALID_ARG);
    CHECK(nb_schedule_core_cancel(NULL, 1u) == NB_SCHEDULE_STATUS_ERR_INVALID_ARG);
    CHECK(nb_schedule_core_tick(NULL, 1000u, fired, 1u) == 0u);
    CHECK(!nb_schedule_core_get(NULL, 1u, &out));
    CHECK(nb_schedule_core_count(NULL) == 0u);

    nb_schedule_core_t core;
    nb_schedule_core_init(&core);
    CHECK(nb_schedule_core_tick(&core, 1000u, NULL, 1u) == 0u);
    CHECK(!nb_schedule_core_get(&core, 1u, NULL));
}

int main(void) {
    test_create_and_get();
    test_create_null_label_is_empty();
    test_create_truncates_long_label();
    test_create_same_id_upserts_not_duplicates();
    test_full_when_all_slots_distinct_ids();
    test_cancel_not_found();
    test_tick_fires_expired_and_clears_slot();
    test_tick_respects_max_fired_and_retries_next_call();
    test_tick_not_yet_due_stays_pending();
    test_null_is_safe();

    if (failures == 0) {
        printf("schedule_core host_test: ok\n");
        return 0;
    }

    printf("schedule_core host_test: %d failure(s)\n", failures);
    return 1;
}
