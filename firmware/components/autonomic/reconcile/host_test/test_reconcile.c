#include "../reconcile.h"

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
    nb_reconcile_state_t r;
    nb_reconcile_init(&r);
    CHECK(nb_reconcile_current_beat(&r) == NB_RECONCILE_BEAT_NONE);
}

/* RFC §5.2: só dispara com vetor negativo E carinho simultâneos -- nenhum
 * dos dois isolado é suficiente. */
static void test_neither_condition_alone_triggers(void)
{
    nb_reconcile_state_t r;

    nb_reconcile_init(&r);
    nb_reconcile_tick(&r, 100u, false, true); /* só carinho, vetor não-negativo */
    CHECK(nb_reconcile_current_beat(&r) == NB_RECONCILE_BEAT_NONE);

    nb_reconcile_init(&r);
    nb_reconcile_tick(&r, 100u, true, false); /* só vetor negativo, sem carinho */
    CHECK(nb_reconcile_current_beat(&r) == NB_RECONCILE_BEAT_NONE);
}

static void test_both_conditions_trigger_gaze_front_first(void)
{
    nb_reconcile_state_t r;
    nb_reconcile_init(&r);

    nb_reconcile_tick(&r, 1u, true, true);
    CHECK(nb_reconcile_current_beat(&r) == NB_RECONCILE_BEAT_GAZE_FRONT);
}

/* Sequência completa nos tempos certos, carinho mantido o tempo todo:
 * GAZE_FRONT(300) -> SMOOTHING(1500) -> CONTENT_SLOW_BLINK_HEART(650) -> NONE. */
static void test_full_sequence_with_caress_sustained_ends_with_heart(void)
{
    nb_reconcile_state_t r;
    nb_reconcile_init(&r);

    nb_reconcile_tick(&r, 1u, true, true);
    CHECK(nb_reconcile_current_beat(&r) == NB_RECONCILE_BEAT_GAZE_FRONT);

    nb_reconcile_tick(&r, 299u, true, true); /* completa os 300ms do ORIENT */
    CHECK(nb_reconcile_current_beat(&r) == NB_RECONCILE_BEAT_SMOOTHING);

    nb_reconcile_tick(&r, 1499u, true, true); /* quase os 1500ms do EXECUTE */
    CHECK(nb_reconcile_current_beat(&r) == NB_RECONCILE_BEAT_SMOOTHING);

    nb_reconcile_tick(&r, 1u, true, true); /* completa -- entra em OUTCOME com carinho ativo */
    CHECK(nb_reconcile_current_beat(&r) == NB_RECONCILE_BEAT_CONTENT_SLOW_BLINK_HEART);

    nb_reconcile_tick(&r, 649u, true, true);
    CHECK(nb_reconcile_current_beat(&r) == NB_RECONCILE_BEAT_CONTENT_SLOW_BLINK_HEART);

    nb_reconcile_tick(&r, 1u, true, true); /* fecha o arco */
    CHECK(nb_reconcile_current_beat(&r) == NB_RECONCILE_BEAT_NONE);
}

/* Carinho para exatamente no instante em que OUTCOME começa: sem coração
 * (snapshot já não estava ativo). */
static void test_outcome_without_caress_has_no_heart(void)
{
    nb_reconcile_state_t r;
    nb_reconcile_init(&r);

    nb_reconcile_tick(&r, 1u, true, true);
    nb_reconcile_tick(&r, 299u, true, true);   /* fim do ORIENT */
    nb_reconcile_tick(&r, 1500u, true, false); /* completa o EXECUTE, mas carinho já caiu */
    CHECK(nb_reconcile_current_beat(&r) == NB_RECONCILE_BEAT_CONTENT_SLOW_BLINK);
}

/* RFC §5.2: aborta limpo se o carinho parar antes da suavização (ORIENT
 * ou EXECUTE) -- volta a NONE, sem ficar pendurado. */
static void test_caress_stopping_during_orient_aborts(void)
{
    nb_reconcile_state_t r;
    nb_reconcile_init(&r);

    nb_reconcile_tick(&r, 1u, true, true);
    CHECK(nb_reconcile_current_beat(&r) == NB_RECONCILE_BEAT_GAZE_FRONT);

    nb_reconcile_tick(&r, 100u, true, false); /* carinho para no meio do ORIENT */
    CHECK(nb_reconcile_current_beat(&r) == NB_RECONCILE_BEAT_NONE);
}

static void test_caress_stopping_during_execute_aborts(void)
{
    nb_reconcile_state_t r;
    nb_reconcile_init(&r);

    nb_reconcile_tick(&r, 1u, true, true);
    nb_reconcile_tick(&r, 299u, true, true); /* entra em EXECUTE */
    CHECK(nb_reconcile_current_beat(&r) == NB_RECONCILE_BEAT_SMOOTHING);

    nb_reconcile_tick(&r, 500u, true, false); /* carinho para no meio da suavização */
    CHECK(nb_reconcile_current_beat(&r) == NB_RECONCILE_BEAT_NONE);
}

/* Vetor deixando de ser negativo no meio do arco não é o gatilho de
 * aborto descrito no RFC (só o carinho parar aborta) -- o arco continua. */
static void test_valence_turning_positive_mid_arc_does_not_abort(void)
{
    nb_reconcile_state_t r;
    nb_reconcile_init(&r);

    nb_reconcile_tick(&r, 1u, true, true);
    nb_reconcile_tick(&r, 299u, false, true); /* vetor já não-negativo, carinho continua */
    CHECK(nb_reconcile_current_beat(&r) == NB_RECONCILE_BEAT_SMOOTHING);
}

/* Sem cooldown: assim que o arco fecha, se as condições ainda valerem
 * (hipoteticamente -- na integração real valence vira positivo e isso
 * não aconteceria), pode iniciar de novo imediatamente. */
static void test_no_cooldown_allows_immediate_retrigger(void)
{
    nb_reconcile_state_t r;
    nb_reconcile_init(&r);

    nb_reconcile_tick(&r, 1u, true, true);
    nb_reconcile_tick(&r, 299u, true, true);
    nb_reconcile_tick(&r, 1500u, true, true);
    nb_reconcile_tick(&r, 650u, true, true);
    CHECK(nb_reconcile_current_beat(&r) == NB_RECONCILE_BEAT_NONE);

    nb_reconcile_tick(&r, 1u, true, true);
    CHECK(nb_reconcile_current_beat(&r) == NB_RECONCILE_BEAT_GAZE_FRONT);
}

static void test_abort_from_outside_clears_to_none(void)
{
    nb_reconcile_state_t r;
    nb_reconcile_init(&r);

    nb_reconcile_tick(&r, 1u, true, true);
    CHECK(nb_reconcile_current_beat(&r) == NB_RECONCILE_BEAT_GAZE_FRONT);

    nb_reconcile_abort(&r);
    CHECK(nb_reconcile_current_beat(&r) == NB_RECONCILE_BEAT_NONE);
}

static void test_null_is_safe(void)
{
    nb_reconcile_init(NULL);
    nb_reconcile_tick(NULL, 10u, true, true);
    nb_reconcile_abort(NULL);
    CHECK(nb_reconcile_current_beat(NULL) == NB_RECONCILE_BEAT_NONE);
}

int main(void)
{
    test_init_is_none();
    test_neither_condition_alone_triggers();
    test_both_conditions_trigger_gaze_front_first();
    test_full_sequence_with_caress_sustained_ends_with_heart();
    test_outcome_without_caress_has_no_heart();
    test_caress_stopping_during_orient_aborts();
    test_caress_stopping_during_execute_aborts();
    test_valence_turning_positive_mid_arc_does_not_abort();
    test_no_cooldown_allows_immediate_retrigger();
    test_abort_from_outside_clears_to_none();
    test_null_is_safe();

    if (failures == 0) {
        printf("reconcile host_test: ok\n");
        return 0;
    }

    printf("reconcile host_test: %d failure(s)\n", failures);
    return 1;
}
