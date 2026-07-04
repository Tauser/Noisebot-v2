#include "../tiny_fsm.h"

#include <stdio.h>

static int failures;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);           \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static int float_eq(float a, float b)
{
    const float diff = (a > b) ? (a - b) : (b - a);
    return diff < 0.0001f;
}

static int transient_is_canonical_clean(const nb_fsm_transient_t *t)
{
    return !t->attentive_motif && !t->speaking && !t->touch_reaction &&
           !t->sleeping_visual && !t->error_icon && !t->safe_mode_icon &&
           float_eq(t->gaze_x, 0.0f) && float_eq(t->gaze_y, 0.0f);
}

/* Uma aresta documentada de BEHAVIOR.md §1. */
typedef struct {
    nb_fsm_state_t from;
    nb_fsm_event_t event;
    nb_fsm_state_t to;
} nb_fsm_edge_t;

static const nb_fsm_edge_t kEdges[] = {
    {NB_FSM_STATE_BOOT, NB_FSM_EVENT_BOOT_OK, NB_FSM_STATE_IDLE},
    {NB_FSM_STATE_BOOT, NB_FSM_EVENT_BOOT_FAIL_CRITICAL, NB_FSM_STATE_SAFE_MODE},
    {NB_FSM_STATE_IDLE, NB_FSM_EVENT_WAKE, NB_FSM_STATE_ATTENTIVE},
    {NB_FSM_STATE_IDLE, NB_FSM_EVENT_SAY_START, NB_FSM_STATE_RESPONDING},
    {NB_FSM_STATE_IDLE, NB_FSM_EVENT_TOUCH, NB_FSM_STATE_TOUCH_REACTING},
    {NB_FSM_STATE_IDLE, NB_FSM_EVENT_SLEEP, NB_FSM_STATE_SLEEPING},
    {NB_FSM_STATE_ATTENTIVE, NB_FSM_EVENT_SAY_START, NB_FSM_STATE_RESPONDING},
    {NB_FSM_STATE_ATTENTIVE, NB_FSM_EVENT_ATTENTIVE_TIMEOUT, NB_FSM_STATE_IDLE},
    {NB_FSM_STATE_ATTENTIVE, NB_FSM_EVENT_TOUCH, NB_FSM_STATE_TOUCH_REACTING},
    {NB_FSM_STATE_RESPONDING, NB_FSM_EVENT_SAY_END, NB_FSM_STATE_IDLE},
    {NB_FSM_STATE_RESPONDING, NB_FSM_EVENT_SAY_FOLLOWUP, NB_FSM_STATE_ATTENTIVE},
    {NB_FSM_STATE_RESPONDING, NB_FSM_EVENT_TOUCH, NB_FSM_STATE_TOUCH_REACTING},
    {NB_FSM_STATE_RESPONDING, NB_FSM_EVENT_SERVER_DROPPED, NB_FSM_STATE_IDLE},
    {NB_FSM_STATE_TOUCH_REACTING, NB_FSM_EVENT_TOUCH_END, NB_FSM_STATE_IDLE},
    {NB_FSM_STATE_SLEEPING, NB_FSM_EVENT_TOUCH, NB_FSM_STATE_IDLE},
    {NB_FSM_STATE_SLEEPING, NB_FSM_EVENT_WAKE_HOUR, NB_FSM_STATE_IDLE},
    {NB_FSM_STATE_SLEEPING, NB_FSM_EVENT_WAKE, NB_FSM_STATE_ATTENTIVE},
    {NB_FSM_STATE_ERROR, NB_FSM_EVENT_RECOVER, NB_FSM_STATE_IDLE},
};

#define EDGE_COUNT (sizeof(kEdges) / sizeof(kEdges[0]))

static void force_state(nb_tiny_fsm_t *fsm, nb_fsm_state_t state)
{
    nb_tiny_fsm_init(fsm);
    fsm->state = state;
}

static int event_is_valid_for(nb_fsm_state_t state, nb_fsm_event_t event)
{
    for (size_t i = 0; i < EDGE_COUNT; ++i) {
        if (kEdges[i].from == state && kEdges[i].event == event) {
            return 1;
        }
    }
    /* Eventos de modo e FAULT/FAULT_CRITICAL valem de qualquer estado
     * (exceto SAFE_MODE para os dois últimos) -- tratados à parte pelos
     * testes dedicados abaixo, não fazem parte da tabela de arestas. */
    return 0;
}

static void test_documented_edges_transition_correctly(void)
{
    for (size_t i = 0; i < EDGE_COUNT; ++i) {
        nb_tiny_fsm_t fsm;

        force_state(&fsm, kEdges[i].from);
        const bool changed = nb_tiny_fsm_apply_event(&fsm, kEdges[i].event);

        CHECK(changed);
        CHECK(nb_tiny_fsm_get_state(&fsm) == kEdges[i].to);
    }
}

static void test_undocumented_state_event_pairs_are_noop(void)
{
    for (int state = 0; state < NB_FSM_STATE_COUNT; ++state) {
        for (int event = 0; event < NB_FSM_EVENT_COUNT; ++event) {
            const nb_fsm_event_t ev = (nb_fsm_event_t)event;

            /* Modo e fault/fault_critical (fora de SAFE_MODE) têm regras
             * próprias, cobertas pelos testes dedicados. */
            if (ev >= NB_FSM_EVENT_MODE_QUIET_ENTER) {
                continue;
            }
            if ((ev == NB_FSM_EVENT_FAULT || ev == NB_FSM_EVENT_FAULT_CRITICAL) &&
                state != NB_FSM_STATE_SAFE_MODE) {
                continue;
            }
            /* Wake em SLEEPING tem regra condicional (modo quiet) coberta
             * à parte -- aqui ele é uma aresta válida (kEdges já cobre). */
            if (event_is_valid_for((nb_fsm_state_t)state, ev)) {
                continue;
            }

            nb_tiny_fsm_t fsm;
            force_state(&fsm, (nb_fsm_state_t)state);
            const bool changed = nb_tiny_fsm_apply_event(&fsm, ev);

            CHECK(!changed);
            CHECK(nb_tiny_fsm_get_state(&fsm) == (nb_fsm_state_t)state);
        }
    }
}

static void test_fault_wins_from_any_state_except_safe_mode(void)
{
    for (int state = 0; state < NB_FSM_STATE_COUNT; ++state) {
        if ((nb_fsm_state_t)state == NB_FSM_STATE_SAFE_MODE) {
            continue;
        }

        nb_tiny_fsm_t fsm;
        force_state(&fsm, (nb_fsm_state_t)state);
        CHECK(nb_tiny_fsm_apply_event(&fsm, NB_FSM_EVENT_FAULT));
        CHECK(nb_tiny_fsm_get_state(&fsm) == NB_FSM_STATE_ERROR);

        force_state(&fsm, (nb_fsm_state_t)state);
        CHECK(nb_tiny_fsm_apply_event(&fsm, NB_FSM_EVENT_FAULT_CRITICAL));
        CHECK(nb_tiny_fsm_get_state(&fsm) == NB_FSM_STATE_SAFE_MODE);
    }
}

static void test_safe_mode_is_sticky(void)
{
    const nb_fsm_event_t all_events[] = {
        NB_FSM_EVENT_BOOT_OK,       NB_FSM_EVENT_WAKE,
        NB_FSM_EVENT_SAY_START,     NB_FSM_EVENT_TOUCH,
        NB_FSM_EVENT_SLEEP,         NB_FSM_EVENT_RECOVER,
        NB_FSM_EVENT_FAULT,         NB_FSM_EVENT_FAULT_CRITICAL,
    };

    for (size_t i = 0; i < sizeof(all_events) / sizeof(all_events[0]); ++i) {
        nb_tiny_fsm_t fsm;
        force_state(&fsm, NB_FSM_STATE_SAFE_MODE);
        const bool changed = nb_tiny_fsm_apply_event(&fsm, all_events[i]);
        CHECK(!changed);
        CHECK(nb_tiny_fsm_get_state(&fsm) == NB_FSM_STATE_SAFE_MODE);
    }
}

static void test_wake_ignored_in_sleeping_quiet_mode(void)
{
    nb_tiny_fsm_t fsm;

    force_state(&fsm, NB_FSM_STATE_SLEEPING);
    nb_tiny_fsm_apply_event(&fsm, NB_FSM_EVENT_MODE_QUIET_ENTER);
    CHECK(!nb_tiny_fsm_apply_event(&fsm, NB_FSM_EVENT_WAKE));
    CHECK(nb_tiny_fsm_get_state(&fsm) == NB_FSM_STATE_SLEEPING);

    /* Sem quiet, wake acorda normalmente. */
    force_state(&fsm, NB_FSM_STATE_SLEEPING);
    CHECK(nb_tiny_fsm_apply_event(&fsm, NB_FSM_EVENT_WAKE));
    CHECK(nb_tiny_fsm_get_state(&fsm) == NB_FSM_STATE_ATTENTIVE);

    /* Touch e wake_hour não são bloqueados por quiet (só wake é). */
    force_state(&fsm, NB_FSM_STATE_SLEEPING);
    nb_tiny_fsm_apply_event(&fsm, NB_FSM_EVENT_MODE_QUIET_ENTER);
    CHECK(nb_tiny_fsm_apply_event(&fsm, NB_FSM_EVENT_TOUCH));
    CHECK(nb_tiny_fsm_get_state(&fsm) == NB_FSM_STATE_IDLE);
}

static void test_modes_persist_across_state_transitions(void)
{
    nb_tiny_fsm_t fsm;

    force_state(&fsm, NB_FSM_STATE_IDLE);
    nb_tiny_fsm_apply_event(&fsm, NB_FSM_EVENT_MODE_QUIET_ENTER);
    nb_tiny_fsm_apply_event(&fsm, NB_FSM_EVENT_MODE_MAINTENANCE_ENTER);
    CHECK(nb_tiny_fsm_get_modes(&fsm) ==
          ((uint32_t)NB_FSM_MODE_QUIET | (uint32_t)NB_FSM_MODE_MAINTENANCE));

    nb_tiny_fsm_apply_event(&fsm, NB_FSM_EVENT_SLEEP);
    CHECK(nb_tiny_fsm_get_state(&fsm) == NB_FSM_STATE_SLEEPING);
    CHECK(nb_tiny_fsm_get_modes(&fsm) ==
          ((uint32_t)NB_FSM_MODE_QUIET | (uint32_t)NB_FSM_MODE_MAINTENANCE));

    nb_tiny_fsm_apply_event(&fsm, NB_FSM_EVENT_MODE_MAINTENANCE_EXIT);
    CHECK(nb_tiny_fsm_get_modes(&fsm) == (uint32_t)NB_FSM_MODE_QUIET);
}

/* Gate H7 (BEHAVIOR.md §1.1, ARCHITECTURE.md §4): toda transição X→IDLE
 * limpa o estado transitório por completo, não importa de onde veio nem
 * quais modos estavam ativos. Cobre 100% das transições X→IDLE
 * documentadas × as combinações de modo. */
static void test_invariant_every_transition_to_idle_is_clean(void)
{
    typedef struct {
        nb_fsm_state_t from;
        nb_fsm_event_t event;
    } to_idle_edge_t;

    static const to_idle_edge_t kToIdle[] = {
        {NB_FSM_STATE_BOOT, NB_FSM_EVENT_BOOT_OK},
        {NB_FSM_STATE_ATTENTIVE, NB_FSM_EVENT_ATTENTIVE_TIMEOUT},
        {NB_FSM_STATE_RESPONDING, NB_FSM_EVENT_SAY_END},
        {NB_FSM_STATE_RESPONDING, NB_FSM_EVENT_SERVER_DROPPED},
        {NB_FSM_STATE_TOUCH_REACTING, NB_FSM_EVENT_TOUCH_END},
        {NB_FSM_STATE_SLEEPING, NB_FSM_EVENT_TOUCH},
        {NB_FSM_STATE_SLEEPING, NB_FSM_EVENT_WAKE_HOUR},
        {NB_FSM_STATE_ERROR, NB_FSM_EVENT_RECOVER},
    };
    static const uint32_t kModeCombos[] = {
        NB_FSM_MODE_NONE,
        (uint32_t)NB_FSM_MODE_QUIET,
        (uint32_t)NB_FSM_MODE_COMPANION,
        (uint32_t)NB_FSM_MODE_MAINTENANCE,
        (uint32_t)NB_FSM_MODE_QUIET | (uint32_t)NB_FSM_MODE_COMPANION |
            (uint32_t)NB_FSM_MODE_MAINTENANCE,
    };

    for (size_t e = 0; e < sizeof(kToIdle) / sizeof(kToIdle[0]); ++e) {
        for (size_t m = 0; m < sizeof(kModeCombos) / sizeof(kModeCombos[0]); ++m) {
            nb_tiny_fsm_t fsm;

            force_state(&fsm, kToIdle[e].from);
            fsm.modes = kModeCombos[m];

            CHECK(nb_tiny_fsm_apply_event(&fsm, kToIdle[e].event));
            CHECK(nb_tiny_fsm_get_state(&fsm) == NB_FSM_STATE_IDLE);
            CHECK(transient_is_canonical_clean(nb_tiny_fsm_get_transient(&fsm)));
            /* Modos não são zerados pelo pouso em IDLE -- só o transiente. */
            CHECK(nb_tiny_fsm_get_modes(&fsm) == kModeCombos[m]);
        }
    }
}

static void test_transient_reflects_dirty_state_before_idle_reset(void)
{
    nb_tiny_fsm_t fsm;

    /* ATTENTIVE liga motif + gaze; RESPONDING (que não mexe em gaze)
     * herda o resíduo; IDLE precisa zerar mesmo assim. */
    force_state(&fsm, NB_FSM_STATE_IDLE);
    nb_tiny_fsm_apply_event(&fsm, NB_FSM_EVENT_WAKE);
    CHECK(nb_tiny_fsm_get_transient(&fsm)->attentive_motif);
    CHECK(!float_eq(nb_tiny_fsm_get_transient(&fsm)->gaze_x, 0.0f));

    nb_tiny_fsm_apply_event(&fsm, NB_FSM_EVENT_SAY_START);
    CHECK(nb_tiny_fsm_get_state(&fsm) == NB_FSM_STATE_RESPONDING);
    CHECK(nb_tiny_fsm_get_transient(&fsm)->speaking);
    CHECK(!nb_tiny_fsm_get_transient(&fsm)->attentive_motif);
    /* resíduo de gaze da ATTENTIVE anterior, RESPONDING não zera sozinho */
    CHECK(!float_eq(nb_tiny_fsm_get_transient(&fsm)->gaze_x, 0.0f));

    nb_tiny_fsm_apply_event(&fsm, NB_FSM_EVENT_SAY_END);
    CHECK(nb_tiny_fsm_get_state(&fsm) == NB_FSM_STATE_IDLE);
    CHECK(transient_is_canonical_clean(nb_tiny_fsm_get_transient(&fsm)));
}

static void test_init_is_deterministic(void)
{
    nb_tiny_fsm_t fsm;

    nb_tiny_fsm_init(&fsm);
    CHECK(nb_tiny_fsm_get_state(&fsm) == NB_FSM_STATE_BOOT);
    CHECK(nb_tiny_fsm_get_modes(&fsm) == NB_FSM_MODE_NONE);
    CHECK(transient_is_canonical_clean(nb_tiny_fsm_get_transient(&fsm)));
}

static void test_null_is_safe(void)
{
    nb_tiny_fsm_init(NULL);
    CHECK(!nb_tiny_fsm_apply_event(NULL, NB_FSM_EVENT_BOOT_OK));
    CHECK(nb_tiny_fsm_get_state(NULL) == NB_FSM_STATE_BOOT);
    CHECK(nb_tiny_fsm_get_modes(NULL) == NB_FSM_MODE_NONE);
    CHECK(nb_tiny_fsm_get_transient(NULL) == NULL);
}

static void test_state_name_covers_all_and_falls_back(void)
{
    for (int i = 0; i < NB_FSM_STATE_COUNT; ++i) {
        CHECK(nb_tiny_fsm_state_name((nb_fsm_state_t)i) != NULL);
    }
    CHECK(nb_tiny_fsm_state_name((nb_fsm_state_t)999) != NULL);
}

int main(void)
{
    test_documented_edges_transition_correctly();
    test_undocumented_state_event_pairs_are_noop();
    test_fault_wins_from_any_state_except_safe_mode();
    test_safe_mode_is_sticky();
    test_wake_ignored_in_sleeping_quiet_mode();
    test_modes_persist_across_state_transitions();
    test_invariant_every_transition_to_idle_is_clean();
    test_transient_reflects_dirty_state_before_idle_reset();
    test_init_is_deterministic();
    test_null_is_safe();
    test_state_name_covers_all_and_falls_back();

    if (failures == 0) {
        printf("tiny_fsm host_test: ok\n");
        return 0;
    }

    printf("tiny_fsm host_test: %d failure(s)\n", failures);
    return 1;
}
