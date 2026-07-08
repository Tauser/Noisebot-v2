#include "../search.h"

#include <stdio.h>

static int failures;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);           \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static void test_init_is_none(void)
{
    nb_search_state_t s;
    nb_search_init(&s, 1u);
    CHECK(nb_search_current_beat(&s) == NB_SEARCH_BEAT_NONE);
    CHECK(nb_search_current_pattern(&s) == NB_SEARCH_PATTERN_COUNT);
    CHECK(nb_search_current_glance_count(&s) == 0u);
}

static void test_stimulus_trigger_starts_orient(void)
{
    nb_search_state_t s;
    nb_search_init(&s, 1u);

    CHECK(nb_search_trigger_stimulus(&s));
    CHECK(nb_search_current_beat(&s) == NB_SEARCH_BEAT_ORIENT);
    CHECK(nb_search_current_pattern(&s) != NB_SEARCH_PATTERN_COUNT);
    CHECK(nb_search_current_glance_count(&s) >= NB_SEARCH_GLANCE_MIN_COUNT &&
          nb_search_current_glance_count(&s) <= NB_SEARCH_GLANCE_MAX_COUNT);
}

/* Estímulo repetido enquanto já roda não reinicia (arc_core recusa). */
static void test_stimulus_trigger_rejected_while_running(void)
{
    nb_search_state_t s;
    nb_search_init(&s, 1u);
    CHECK(nb_search_trigger_stimulus(&s));
    CHECK(!nb_search_trigger_stimulus(&s));
}

/* Roda uma corrida inteira até IDLE e devolve se achou (via beats
 * observados) -- ajuda os testes de sequência/variedade sem duplicar
 * lógica de avanço em cada teste. */
static bool run_to_completion(nb_search_state_t *s, bool *out_saw_found,
                              bool *out_saw_not_found)
{
    *out_saw_found = false;
    *out_saw_not_found = false;

    uint32_t ticks = 0;
    const uint32_t max_ticks = 100000u;
    while (nb_search_current_beat(s) != NB_SEARCH_BEAT_NONE || ticks == 0u) {
        nb_search_tick(s, 10u);
        const nb_search_beat_t beat = nb_search_current_beat(s);
        if (beat == NB_SEARCH_BEAT_FOUND) {
            *out_saw_found = true;
        }
        if (beat == NB_SEARCH_BEAT_NOT_FOUND_BLINK || beat == NB_SEARCH_BEAT_NOT_FOUND_SIGH) {
            *out_saw_not_found = true;
        }
        ++ticks;
        if (ticks >= max_ticks) {
            return false; /* nunca devia acontecer -- ver test_never_stuck de arc_core */
        }
        if (beat == NB_SEARCH_BEAT_NONE && ticks > 1u) {
            break;
        }
    }
    return true;
}

/* RFC §5.2 + plano S3.8 item 6: sempre um dos dois desfechos, nunca os
 * dois na mesma corrida. */
static void test_exactly_one_outcome_per_run(void)
{
    for (uint32_t seed = 1u; seed <= 30u; ++seed) {
        nb_search_state_t s;
        nb_search_init(&s, seed);
        CHECK(nb_search_trigger_stimulus(&s));

        bool saw_found = false, saw_not_found = false;
        CHECK(run_to_completion(&s, &saw_found, &saw_not_found));
        CHECK(saw_found != saw_not_found); /* XOR -- exatamente um dos dois */
    }
}

/* Padrão de busca nunca repete o mesmo 2x seguidas. */
static void test_pattern_never_repeats_consecutively(void)
{
    nb_search_state_t s;
    nb_search_init(&s, 7u);

    nb_search_pattern_t prev = NB_SEARCH_PATTERN_COUNT;
    for (int i = 0; i < 200; ++i) {
        CHECK(nb_search_trigger_stimulus(&s));
        const nb_search_pattern_t current = nb_search_current_pattern(&s);
        CHECK(current != NB_SEARCH_PATTERN_COUNT);
        if (prev != NB_SEARCH_PATTERN_COUNT) {
            CHECK(current != prev);
        }
        prev = current;

        bool saw_found, saw_not_found;
        run_to_completion(&s, &saw_found, &saw_not_found);
    }
}

/* Tédio: primeiro disparo sempre permitido; um segundo dentro de 1h é
 * recusado; depois de 1h, permitido de novo. */
static void test_boredom_rate_limited_to_once_per_hour(void)
{
    nb_search_state_t s;
    nb_search_init(&s, 1u);

    CHECK(nb_search_trigger_boredom(&s, 0u));

    bool saw_found, saw_not_found;
    run_to_completion(&s, &saw_found, &saw_not_found);

    CHECK(!nb_search_trigger_boredom(&s, 1000u));                          /* 1s depois -- não */
    CHECK(!nb_search_trigger_boredom(&s, NB_SEARCH_BOREDOM_MIN_INTERVAL_MS - 1u)); /* quase 1h -- não */
    CHECK(nb_search_trigger_boredom(&s, NB_SEARCH_BOREDOM_MIN_INTERVAL_MS));       /* 1h exata -- sim */
}

/* Estímulo direto não compartilha o limite de taxa do tédio -- pode
 * disparar de novo assim que a corrida anterior termina, mesmo dentro de
 * 1h. */
static void test_stimulus_trigger_not_rate_limited(void)
{
    nb_search_state_t s;
    nb_search_init(&s, 1u);

    CHECK(nb_search_trigger_stimulus(&s));
    bool saw_found, saw_not_found;
    run_to_completion(&s, &saw_found, &saw_not_found);

    CHECK(nb_search_trigger_stimulus(&s)); /* imediatamente depois -- sem limite */
}

static void test_abort_clears_to_none(void)
{
    nb_search_state_t s;
    nb_search_init(&s, 1u);
    CHECK(nb_search_trigger_stimulus(&s));
    CHECK(nb_search_current_beat(&s) == NB_SEARCH_BEAT_ORIENT);

    nb_search_abort(&s);
    CHECK(nb_search_current_beat(&s) == NB_SEARCH_BEAT_NONE);
    CHECK(nb_search_trigger_stimulus(&s)); /* pode retentar imediatamente */
}

static void test_null_is_safe(void)
{
    nb_search_init(NULL, 1u);
    CHECK(!nb_search_trigger_stimulus(NULL));
    CHECK(!nb_search_trigger_boredom(NULL, 0u));
    nb_search_tick(NULL, 10u);
    nb_search_abort(NULL);
    CHECK(nb_search_current_beat(NULL) == NB_SEARCH_BEAT_NONE);
    CHECK(nb_search_current_pattern(NULL) == NB_SEARCH_PATTERN_COUNT);
    CHECK(nb_search_current_glance_count(NULL) == 0u);
}

int main(void)
{
    test_init_is_none();
    test_stimulus_trigger_starts_orient();
    test_stimulus_trigger_rejected_while_running();
    test_exactly_one_outcome_per_run();
    test_pattern_never_repeats_consecutively();
    test_boredom_rate_limited_to_once_per_hour();
    test_stimulus_trigger_not_rate_limited();
    test_abort_clears_to_none();
    test_null_is_safe();

    if (failures == 0) {
        printf("search host_test: ok\n");
        return 0;
    }

    printf("search host_test: %d failure(s)\n", failures);
    return 1;
}
