#include "../arc_core.h"

#include <stddef.h>
#include <stdio.h>

static int failures;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);           \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static void test_init_is_idle(void)
{
    nb_arc_state_t arc;
    nb_arc_core_init(&arc);

    CHECK(arc.phase == NB_ARC_PHASE_IDLE);
    CHECK(!nb_arc_core_is_running(&arc));
    CHECK(!nb_arc_core_in_cooldown(&arc));
}

static void test_full_sequence_reaches_outcome_then_idle(void)
{
    nb_arc_state_t arc;
    nb_arc_core_init(&arc);

    CHECK(nb_arc_core_start(&arc, 300u, 1000u, 500u, 60000u));
    CHECK(arc.phase == NB_ARC_PHASE_ORIENT);

    nb_arc_core_tick(&arc, 300u);
    CHECK(arc.phase == NB_ARC_PHASE_EXECUTE);

    nb_arc_core_tick(&arc, 1000u);
    CHECK(arc.phase == NB_ARC_PHASE_OUTCOME);

    /* Ainda não esgotou o desfecho -- continua em OUTCOME. */
    nb_arc_core_tick(&arc, 400u);
    CHECK(arc.phase == NB_ARC_PHASE_OUTCOME);

    nb_arc_core_tick(&arc, 100u);
    CHECK(arc.phase == NB_ARC_PHASE_IDLE);
    CHECK(!nb_arc_core_is_running(&arc));
    CHECK(nb_arc_core_in_cooldown(&arc));
}

/* RFC §5.2: "nada termina simplesmente parando" -- nunca fica pendurado
 * numa fase intermediária por mais tempo do que a soma das durações,
 * qualquer que seja o tamanho do passo de tick. */
static void test_never_stuck_mid_phase_regardless_of_step_size(void)
{
    const uint32_t steps[] = {1u, 7u, 50u, 999u};

    for (size_t s = 0; s < sizeof(steps) / sizeof(steps[0]); ++s) {
        nb_arc_state_t arc;
        nb_arc_core_init(&arc);
        CHECK(nb_arc_core_start(&arc, 300u, 1000u, 500u, 60000u));

        uint32_t ticks = 0;
        const uint32_t max_ticks = 10000u;
        while (nb_arc_core_is_running(&arc) && ticks < max_ticks) {
            nb_arc_core_tick(&arc, steps[s]);
            ++ticks;
        }
        CHECK(!nb_arc_core_is_running(&arc));
        CHECK(ticks < max_ticks);
    }
}

/* Cooldown bloqueia reinício imediato; some depois do próprio cooldown_ms. */
static void test_cooldown_blocks_restart_until_it_expires(void)
{
    nb_arc_state_t arc;
    nb_arc_core_init(&arc);

    CHECK(nb_arc_core_start(&arc, 100u, 100u, 100u, 5000u));
    nb_arc_core_tick(&arc, 300u); /* completa o arco inteiro */
    CHECK(arc.phase == NB_ARC_PHASE_IDLE);
    CHECK(nb_arc_core_in_cooldown(&arc));

    CHECK(!nb_arc_core_start(&arc, 100u, 100u, 100u, 5000u));
    CHECK(arc.phase == NB_ARC_PHASE_IDLE); /* start recusado não muda nada */

    nb_arc_core_tick(&arc, 4999u); /* ainda dentro do cooldown */
    CHECK(nb_arc_core_in_cooldown(&arc));
    CHECK(!nb_arc_core_start(&arc, 100u, 100u, 100u, 5000u));

    nb_arc_core_tick(&arc, 2u); /* cooldown expira */
    CHECK(!nb_arc_core_in_cooldown(&arc));
    CHECK(nb_arc_core_start(&arc, 100u, 100u, 100u, 5000u));
}

/* H7 estendida (RFC §5.2): entrada em IDLE aborta em qualquer fase, sem
 * cooldown -- pode retentar imediatamente. */
static void test_abort_clears_from_any_phase_without_cooldown(void)
{
    const nb_arc_phase_t phases_to_reach[] = {NB_ARC_PHASE_ORIENT, NB_ARC_PHASE_EXECUTE,
                                              NB_ARC_PHASE_OUTCOME};
    /* dt acumulado desde o start (não a duração de cada fase isolada):
     * EXECUTE só começa após orient_ms(300); OUTCOME só após
     * orient_ms+execute_ms(300+1000=1300). */
    const uint32_t dt_to_reach[] = {0u, 300u, 1300u};

    for (size_t i = 0; i < sizeof(phases_to_reach) / sizeof(phases_to_reach[0]); ++i) {
        nb_arc_state_t arc;
        nb_arc_core_init(&arc);
        CHECK(nb_arc_core_start(&arc, 300u, 1000u, 500u, 60000u));
        if (dt_to_reach[i] > 0u) {
            nb_arc_core_tick(&arc, dt_to_reach[i]);
        }
        CHECK(arc.phase == phases_to_reach[i]);

        nb_arc_core_abort(&arc);
        CHECK(arc.phase == NB_ARC_PHASE_IDLE);
        CHECK(!nb_arc_core_in_cooldown(&arc));
        CHECK(nb_arc_core_start(&arc, 300u, 1000u, 500u, 60000u));
    }
}

static void test_abort_when_already_idle_is_noop(void)
{
    nb_arc_state_t arc;
    nb_arc_core_init(&arc);
    nb_arc_core_abort(&arc);
    CHECK(arc.phase == NB_ARC_PHASE_IDLE);
}

static void test_start_rejected_while_already_running(void)
{
    nb_arc_state_t arc;
    nb_arc_core_init(&arc);
    CHECK(nb_arc_core_start(&arc, 300u, 1000u, 500u, 60000u));
    CHECK(!nb_arc_core_start(&arc, 1u, 1u, 1u, 1u));
    CHECK(arc.phase == NB_ARC_PHASE_ORIENT); /* segunda chamada não pisou no estado */
}

static void test_null_is_safe(void)
{
    nb_arc_core_init(NULL);
    nb_arc_core_tick(NULL, 100u);
    nb_arc_core_abort(NULL);
    CHECK(!nb_arc_core_is_running(NULL));
    CHECK(!nb_arc_core_in_cooldown(NULL));
    CHECK(!nb_arc_core_start(NULL, 1u, 1u, 1u, 1u));
}

int main(void)
{
    test_init_is_idle();
    test_full_sequence_reaches_outcome_then_idle();
    test_never_stuck_mid_phase_regardless_of_step_size();
    test_cooldown_blocks_restart_until_it_expires();
    test_abort_clears_from_any_phase_without_cooldown();
    test_abort_when_already_idle_is_noop();
    test_start_rejected_while_already_running();
    test_null_is_safe();

    if (failures == 0) {
        printf("arc_core host_test: ok\n");
        return 0;
    }

    printf("arc_core host_test: %d failure(s)\n", failures);
    return 1;
}
