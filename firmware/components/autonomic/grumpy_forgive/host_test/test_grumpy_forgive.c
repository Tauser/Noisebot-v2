#include "../grumpy_forgive.h"

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
    nb_grumpy_forgive_state_t g;
    nb_grumpy_forgive_init(&g);
    CHECK(nb_grumpy_forgive_current_beat(&g) == NB_GRUMPY_FORGIVE_BEAT_NONE);
}

/* RFC §5.2: motion nunca dispara com menos de 3 TAP na janela. */
static void test_two_taps_never_triggers(void)
{
    nb_grumpy_forgive_state_t g;
    nb_grumpy_forgive_init(&g);

    CHECK(!nb_grumpy_forgive_on_tap(&g, 1000u));
    CHECK(!nb_grumpy_forgive_on_tap(&g, 1500u));
    CHECK(nb_grumpy_forgive_current_beat(&g) == NB_GRUMPY_FORGIVE_BEAT_NONE);
}

/* 3 taps espalhados por mais de 10s não disparam -- janela deslizante de
 * verdade, não só contagem total. */
static void test_three_taps_outside_window_never_triggers(void)
{
    nb_grumpy_forgive_state_t g;
    nb_grumpy_forgive_init(&g);

    CHECK(!nb_grumpy_forgive_on_tap(&g, 0u));
    CHECK(!nb_grumpy_forgive_on_tap(&g, 6000u));
    CHECK(!nb_grumpy_forgive_on_tap(&g, 15000u)); /* 15s desde o 1º -- fora da janela de 10s */
}

/* 3 taps dentro de 10s dispara o arco. */
static void test_three_taps_within_window_triggers(void)
{
    nb_grumpy_forgive_state_t g;
    nb_grumpy_forgive_init(&g);

    CHECK(!nb_grumpy_forgive_on_tap(&g, 0u));
    CHECK(!nb_grumpy_forgive_on_tap(&g, 3000u));
    CHECK(nb_grumpy_forgive_on_tap(&g, 9000u)); /* 3 taps em 9s -- dentro da janela de 10s */
}

/* RFC §5.2: sequência completa nos tempos certos --
 * ANGRY(1200ms) -> BLINK_GAZE_AWAY(800ms) -> NEUTRAL_HAPPY(400ms) -> NONE. */
static void test_full_sequence_beats_in_order(void)
{
    nb_grumpy_forgive_state_t g;
    nb_grumpy_forgive_init(&g);

    CHECK(!nb_grumpy_forgive_on_tap(&g, 0u));
    CHECK(!nb_grumpy_forgive_on_tap(&g, 100u));
    CHECK(nb_grumpy_forgive_on_tap(&g, 200u));

    /* orient_ms=0 -- primeiro tick já cascateia pra EXECUTE. */
    nb_grumpy_forgive_tick(&g, 1u);
    CHECK(nb_grumpy_forgive_current_beat(&g) == NB_GRUMPY_FORGIVE_BEAT_ANGRY);

    nb_grumpy_forgive_tick(&g, 1199u); /* completa os 1200ms do EXECUTE */
    CHECK(nb_grumpy_forgive_current_beat(&g) == NB_GRUMPY_FORGIVE_BEAT_BLINK_GAZE_AWAY);

    nb_grumpy_forgive_tick(&g, 799u); /* ainda dentro dos 800ms de gaze */
    CHECK(nb_grumpy_forgive_current_beat(&g) == NB_GRUMPY_FORGIVE_BEAT_BLINK_GAZE_AWAY);

    nb_grumpy_forgive_tick(&g, 1u); /* cruza pra NEUTRAL_HAPPY */
    CHECK(nb_grumpy_forgive_current_beat(&g) == NB_GRUMPY_FORGIVE_BEAT_NEUTRAL_HAPPY);

    nb_grumpy_forgive_tick(&g, 399u); /* ainda dentro dos 400ms finais */
    CHECK(nb_grumpy_forgive_current_beat(&g) == NB_GRUMPY_FORGIVE_BEAT_NEUTRAL_HAPPY);

    nb_grumpy_forgive_tick(&g, 1u); /* fecha o arco -- volta a NONE */
    CHECK(nb_grumpy_forgive_current_beat(&g) == NB_GRUMPY_FORGIVE_BEAT_NONE);
}

/* Cooldown bloqueia retrigger imediato mesmo com 3 novos taps válidos. */
static void test_cooldown_blocks_retrigger(void)
{
    nb_grumpy_forgive_state_t g;
    nb_grumpy_forgive_init(&g);

    CHECK(!nb_grumpy_forgive_on_tap(&g, 0u));
    CHECK(!nb_grumpy_forgive_on_tap(&g, 100u));
    CHECK(nb_grumpy_forgive_on_tap(&g, 200u));

    /* Nova rajada de 3 taps enquanto o arco ainda roda/está em cooldown --
     * nenhuma reinicia. */
    CHECK(!nb_grumpy_forgive_on_tap(&g, 300u));
    CHECK(!nb_grumpy_forgive_on_tap(&g, 400u));
    CHECK(!nb_grumpy_forgive_on_tap(&g, 500u));
}

/* Entrada em IDLE aborta em qualquer fase, sem cooldown -- pode retentar. */
static void test_abort_clears_and_allows_immediate_retrigger(void)
{
    nb_grumpy_forgive_state_t g;
    nb_grumpy_forgive_init(&g);

    CHECK(!nb_grumpy_forgive_on_tap(&g, 0u));
    CHECK(!nb_grumpy_forgive_on_tap(&g, 100u));
    CHECK(nb_grumpy_forgive_on_tap(&g, 200u));
    nb_grumpy_forgive_tick(&g, 500u);
    CHECK(nb_grumpy_forgive_current_beat(&g) == NB_GRUMPY_FORGIVE_BEAT_ANGRY);

    nb_grumpy_forgive_abort(&g);
    CHECK(nb_grumpy_forgive_current_beat(&g) == NB_GRUMPY_FORGIVE_BEAT_NONE);

    /* 3 taps novos, mesmo now_ms (o anel já tinha 3 do disparo anterior --
     * reforça que o abort não deixou lixo que force outro comportamento). */
    CHECK(!nb_grumpy_forgive_on_tap(&g, 100600u));
    CHECK(!nb_grumpy_forgive_on_tap(&g, 100700u));
    CHECK(nb_grumpy_forgive_on_tap(&g, 100800u));
}

static void test_null_is_safe(void)
{
    nb_grumpy_forgive_init(NULL);
    CHECK(!nb_grumpy_forgive_on_tap(NULL, 0u));
    nb_grumpy_forgive_tick(NULL, 10u);
    nb_grumpy_forgive_abort(NULL);
    CHECK(nb_grumpy_forgive_current_beat(NULL) == NB_GRUMPY_FORGIVE_BEAT_NONE);
}

int main(void)
{
    test_init_is_none();
    test_two_taps_never_triggers();
    test_three_taps_outside_window_never_triggers();
    test_three_taps_within_window_triggers();
    test_full_sequence_beats_in_order();
    test_cooldown_blocks_retrigger();
    test_abort_clears_and_allows_immediate_retrigger();
    test_null_is_safe();

    if (failures == 0) {
        printf("grumpy_forgive host_test: ok\n");
        return 0;
    }

    printf("grumpy_forgive host_test: %d failure(s)\n", failures);
    return 1;
}
