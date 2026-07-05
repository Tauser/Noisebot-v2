#include "../reflex_engine.h"

#include <stdio.h>

static int failures;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);           \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static void test_init_is_unclaimed(void) {
    nb_reflex_engine_t engine;

    nb_reflex_engine_init(&engine);
    CHECK(nb_reflex_engine_get_active_priority(&engine, 0u) == NB_REFLEX_PRIORITY_UNCLAIMED);
}

static void test_touch_tap_claims_p1_then_expires(void) {
    nb_reflex_engine_t engine;
    nb_reflex_reaction_t reaction;

    nb_reflex_engine_init(&engine);
    nb_reflex_engine_on_stimulus(&engine, NB_REFLEX_STIMULUS_TOUCH_TAP, 1000u, &reaction);

    CHECK(reaction.active_priority == NB_REFLEX_PRIORITY_TOUCH);
    CHECK(reaction.fsm_event == NB_FSM_EVENT_TOUCH);
    CHECK(reaction.has_affect_delta);
    CHECK(reaction.delta_valence > 0.0f);

    CHECK(nb_reflex_engine_get_active_priority(&engine, 1399u) == NB_REFLEX_PRIORITY_TOUCH);
    CHECK(nb_reflex_engine_get_active_priority(&engine, 1400u) == NB_REFLEX_PRIORITY_UNCLAIMED);
}

/* Conflito touch×idle: enquanto a claim P1 estiver ativa, nenhuma banda
 * inferior pode vencer -- a arbitragem é sempre "primeira banda ativa". */
static void test_touch_suppresses_idle_without_destroying_it(void) {
    nb_reflex_engine_t engine;
    nb_reflex_reaction_t reaction;

    nb_reflex_engine_init(&engine);
    nb_reflex_engine_on_stimulus(&engine, NB_REFLEX_STIMULUS_TOUCH_LONG, 0u, &reaction);
    CHECK(nb_reflex_engine_get_active_priority(&engine, 0u) == NB_REFLEX_PRIORITY_TOUCH);

    /* idle_engine/emotion_core continuam vivos por fora (P4/P5), o
     * reflex_engine só precisa reportar que P1 está no controle agora. */
    CHECK(nb_reflex_engine_get_active_priority(&engine, 2000u) == NB_REFLEX_PRIORITY_TOUCH);

    /* Ao expirar, a banda "de baixo" reaparece sozinha -- não foi destruída,
     * só deixou de ser suprimida (reportado como UNCLAIMED, a casca resolve
     * P4/P5/P6 de novo). */
    CHECK(nb_reflex_engine_get_active_priority(&engine, 3000u) == NB_REFLEX_PRIORITY_UNCLAIMED);
}

/* Conflito de safety×touch: MOTION_FAULT (P0) sempre vence toque (P1)
 * mesmo que o toque tenha sido levantado depois, dentro da mesma janela. */
static void test_safety_beats_touch_even_if_touch_is_more_recent(void) {
    nb_reflex_engine_t engine;
    nb_reflex_reaction_t reaction;

    nb_reflex_engine_init(&engine);
    nb_reflex_engine_on_stimulus(&engine, NB_REFLEX_STIMULUS_MOTION_FAULT, 0u, &reaction);
    nb_reflex_engine_on_stimulus(&engine, NB_REFLEX_STIMULUS_TOUCH_TAP, 100u, &reaction);

    CHECK(nb_reflex_engine_get_active_priority(&engine, 100u) == NB_REFLEX_PRIORITY_SAFETY);
}

/* Empate na mesma banda: o estímulo mais recente vence (renova a claim). */
static void test_same_band_conflict_most_recent_wins(void) {
    nb_reflex_engine_t engine;
    nb_reflex_reaction_t reaction;

    nb_reflex_engine_init(&engine);
    nb_reflex_engine_on_stimulus(&engine, NB_REFLEX_STIMULUS_TOUCH_TAP, 0u, &reaction);
    CHECK(nb_reflex_engine_get_active_priority(&engine, 399u) == NB_REFLEX_PRIORITY_TOUCH);

    /* TOUCH_LONG chega antes do TAP expirar e renova a janela da banda P1. */
    nb_reflex_engine_on_stimulus(&engine, NB_REFLEX_STIMULUS_TOUCH_LONG, 300u, &reaction);
    CHECK(nb_reflex_engine_get_active_priority(&engine, 399u) == NB_REFLEX_PRIORITY_TOUCH);
    CHECK(nb_reflex_engine_get_active_priority(&engine, 3299u) == NB_REFLEX_PRIORITY_TOUCH);
    CHECK(nb_reflex_engine_get_active_priority(&engine, 3300u) == NB_REFLEX_PRIORITY_UNCLAIMED);
}

/* Toque contínuo: TAP/LONG chegam discretos (via on_stimulus, disparado
 * pela casca a partir dos eventos one-shot do touch_service); a partir daí
 * o tick() reclassifica por duração real -- WARM_PULSE 3-8s (repetindo a
 * cada 4s), DEEP 8-15s, CARESS >15s. */
static void test_tick_reclassifies_sustained_touch_by_duration(void) {
    nb_reflex_engine_t engine;
    nb_reflex_reaction_t reaction;

    nb_reflex_engine_init(&engine);

    /* Ainda em LONG_PRESS (duração 1s) -- tick não deve mexer na fase. */
    nb_reflex_engine_tick(&engine, 1000u, true, 1000u, &reaction);
    CHECK(reaction.fsm_event == NB_FSM_EVENT_COUNT);

    /* Cruza 3s: entra em WARM_PULSE. */
    nb_reflex_engine_tick(&engine, 2001u, true, 3001u, &reaction);
    CHECK(reaction.fsm_event == NB_FSM_EVENT_TOUCH);
    CHECK(reaction.delta_arousal == 0.0f);

    /* Continua em WARM_PULSE sem ainda completar o período de repulso de
     * 4s -- não deve reemitir estímulo. */
    nb_reflex_engine_tick(&engine, 1000u, true, 4001u, &reaction);
    CHECK(reaction.fsm_event == NB_FSM_EVENT_COUNT);

    /* Cruza 8s: entra em DEEP. */
    nb_reflex_engine_tick(&engine, 4000u, true, 8001u, &reaction);
    CHECK(reaction.fsm_event == NB_FSM_EVENT_TOUCH);
    CHECK(reaction.delta_arousal < 0.0f);

    /* Cruza 15s: entra em CARESS. */
    nb_reflex_engine_tick(&engine, 7000u, true, 15001u, &reaction);
    CHECK(reaction.fsm_event == NB_FSM_EVENT_TOUCH);
    CHECK(reaction.delta_valence > 0.8f);

    /* Solta o toque: dispara TOUCH_RELEASE (borda de descida). */
    nb_reflex_engine_tick(&engine, 100u, false, 0u, &reaction);
    CHECK(reaction.fsm_event == NB_FSM_EVENT_TOUCH_END);
}

/* Invariante X→IDLE (ARCHITECTURE.md §4, BEHAVIOR.md §1.1): reflex_engine
 * nunca disputa com tiny_fsm -- force_clear zera a pilha imediatamente,
 * mesmo com claims longe de expirar. */
static void test_force_clear_zeroes_stack_regardless_of_expiry(void) {
    nb_reflex_engine_t engine;
    nb_reflex_reaction_t reaction;

    nb_reflex_engine_init(&engine);
    nb_reflex_engine_on_stimulus(&engine, NB_REFLEX_STIMULUS_MOTION_FAULT, 0u, &reaction);
    CHECK(nb_reflex_engine_get_active_priority(&engine, 100u) == NB_REFLEX_PRIORITY_SAFETY);

    nb_reflex_engine_force_clear(&engine);
    CHECK(nb_reflex_engine_get_active_priority(&engine, 100u) == NB_REFLEX_PRIORITY_UNCLAIMED);

    /* Depois do clear, um novo estímulo funciona normalmente. */
    nb_reflex_engine_on_stimulus(&engine, NB_REFLEX_STIMULUS_TOUCH_TAP, 200u, &reaction);
    CHECK(nb_reflex_engine_get_active_priority(&engine, 200u) == NB_REFLEX_PRIORITY_TOUCH);
}

static void test_unknown_stimulus_is_noop(void) {
    nb_reflex_engine_t engine;
    nb_reflex_reaction_t reaction;

    nb_reflex_engine_init(&engine);
    nb_reflex_engine_on_stimulus(&engine, NB_REFLEX_STIMULUS_COUNT, 0u, &reaction);
    CHECK(reaction.active_priority == NB_REFLEX_PRIORITY_UNCLAIMED);
    CHECK(reaction.fsm_event == NB_FSM_EVENT_COUNT);
    CHECK(!reaction.has_affect_delta);
}

static void test_null_is_safe(void) {
    nb_reflex_reaction_t reaction;

    nb_reflex_engine_init(NULL);
    nb_reflex_engine_on_stimulus(NULL, NB_REFLEX_STIMULUS_TOUCH_TAP, 0u, &reaction);
    nb_reflex_engine_on_stimulus(NULL, NB_REFLEX_STIMULUS_TOUCH_TAP, 0u, NULL);
    nb_reflex_engine_tick(NULL, 10u, true, 0u, &reaction);
    nb_reflex_engine_force_clear(NULL);
    CHECK(nb_reflex_engine_get_active_priority(NULL, 0u) == NB_REFLEX_PRIORITY_UNCLAIMED);
}

int main(void) {
    test_init_is_unclaimed();
    test_touch_tap_claims_p1_then_expires();
    test_touch_suppresses_idle_without_destroying_it();
    test_safety_beats_touch_even_if_touch_is_more_recent();
    test_same_band_conflict_most_recent_wins();
    test_tick_reclassifies_sustained_touch_by_duration();
    test_force_clear_zeroes_stack_regardless_of_expiry();
    test_unknown_stimulus_is_noop();
    test_null_is_safe();

    if (failures == 0) {
        printf("reflex_engine host_test: ok\n");
        return 0;
    }

    printf("reflex_engine host_test: %d failure(s)\n", failures);
    return 1;
}
