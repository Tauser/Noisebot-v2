#include "../peak_core.h"

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

static void test_init_is_none(void)
{
    nb_peak_state_t p;
    nb_peak_core_init(&p);
    CHECK(nb_peak_core_active_mechanism(&p) == NB_PEAK_MECHANISM_NONE);
    CHECK(nb_peak_core_fade_scale(&p) == 0.0f);
    CHECK(!nb_peak_core_blink_should_pause(&p));
}

/* Fora do limiar/arco (requested==NONE) nunca aparece nada. */
static void test_no_request_never_activates(void)
{
    nb_peak_state_t p;
    nb_peak_core_init(&p);
    for (int i = 0; i < 100; ++i) {
        nb_peak_core_tick(&p, 100u, NB_PEAK_MECHANISM_NONE);
        CHECK(nb_peak_core_active_mechanism(&p) == NB_PEAK_MECHANISM_NONE);
    }
}

static void test_request_activates_immediately(void)
{
    nb_peak_state_t p;
    nb_peak_core_init(&p);
    nb_peak_core_tick(&p, 1u, NB_PEAK_MECHANISM_HEART);
    CHECK(nb_peak_core_active_mechanism(&p) == NB_PEAK_MECHANISM_HEART);
}

/* Nunca 2 picos simultâneos: um pedido concorrente de outro mecanismo
 * enquanto um já está ativo é ignorado até o slot liberar. */
static void test_never_two_peaks_simultaneously(void)
{
    nb_peak_state_t p;
    nb_peak_core_init(&p);

    nb_peak_core_tick(&p, 1u, NB_PEAK_MECHANISM_HEART);
    CHECK(nb_peak_core_active_mechanism(&p) == NB_PEAK_MECHANISM_HEART);

    /* Pedido concorrente de STARS enquanto HEART está ativo -- ignorado. */
    nb_peak_core_tick(&p, 100u, NB_PEAK_MECHANISM_STARS);
    CHECK(nb_peak_core_active_mechanism(&p) == NB_PEAK_MECHANISM_HEART);
}

/* 1-2s com fade: expira sozinho depois de NB_PEAK_HOLD_MS mesmo que o
 * pedido continue. */
static void test_expires_after_hold_even_if_still_requested(void)
{
    nb_peak_state_t p;
    nb_peak_core_init(&p);

    nb_peak_core_tick(&p, 1u, NB_PEAK_MECHANISM_HEART);
    nb_peak_core_tick(&p, NB_PEAK_HOLD_MS - 2u, NB_PEAK_MECHANISM_HEART);
    CHECK(nb_peak_core_active_mechanism(&p) == NB_PEAK_MECHANISM_HEART);

    nb_peak_core_tick(&p, 2u, NB_PEAK_MECHANISM_HEART); /* cruza o hold */
    CHECK(nb_peak_core_active_mechanism(&p) == NB_PEAK_MECHANISM_NONE);
}

/* Fade: sobe em NB_PEAK_FADE_MS, platô em 1.0, desce nos últimos
 * NB_PEAK_FADE_MS antes do fim do hold. */
static void test_fade_envelope_shape(void)
{
    nb_peak_state_t p;
    nb_peak_core_init(&p);
    nb_peak_core_tick(&p, 1u, NB_PEAK_MECHANISM_TEARS); /* ativa, elapsed=0 */

    nb_peak_core_tick(&p, NB_PEAK_FADE_MS / 2u, NB_PEAK_MECHANISM_TEARS); /* elapsed=150 -- subindo */
    CHECK(nb_peak_core_fade_scale(&p) > 0.0f && nb_peak_core_fade_scale(&p) < 1.0f);

    nb_peak_core_tick(&p, NB_PEAK_FADE_MS / 2u, NB_PEAK_MECHANISM_TEARS); /* elapsed=300 -- platô */
    CHECK(nb_peak_core_fade_scale(&p) >= 0.99f);

    nb_peak_core_tick(&p, NB_PEAK_HOLD_MS - 2u * NB_PEAK_FADE_MS,
                      NB_PEAK_MECHANISM_TEARS); /* elapsed=1200 -- ainda platô, borda de saída */
    CHECK(nb_peak_core_fade_scale(&p) >= 0.99f);

    nb_peak_core_tick(&p, NB_PEAK_FADE_MS / 2u, NB_PEAK_MECHANISM_TEARS); /* elapsed=1350 -- descendo */
    CHECK(nb_peak_core_fade_scale(&p) > 0.0f && nb_peak_core_fade_scale(&p) < 1.0f);
}

/* ZZZ persiste sem timeout enquanto pedido, sem fade (escala sempre 1.0). */
static void test_zzz_persists_without_timeout_or_fade(void)
{
    nb_peak_state_t p;
    nb_peak_core_init(&p);
    nb_peak_core_tick(&p, 1u, NB_PEAK_MECHANISM_ZZZ);

    for (int i = 0; i < 100; ++i) {
        nb_peak_core_tick(&p, 1000u, NB_PEAK_MECHANISM_ZZZ); /* bem além do hold normal */
        CHECK(nb_peak_core_active_mechanism(&p) == NB_PEAK_MECHANISM_ZZZ);
        CHECK(nb_peak_core_fade_scale(&p) == 1.0f);
    }

    /* Some assim que deixa de ser pedido. */
    nb_peak_core_tick(&p, 10u, NB_PEAK_MECHANISM_NONE);
    CHECK(nb_peak_core_active_mechanism(&p) == NB_PEAK_MECHANISM_NONE);
}

/* Entrada em IDLE limpa qualquer mecanismo ativo (H7). */
static void test_reset_transient_clears_any_mechanism(void)
{
    const nb_peak_mechanism_t mechanisms[] = {NB_PEAK_MECHANISM_HEART, NB_PEAK_MECHANISM_ZZZ,
                                              NB_PEAK_MECHANISM_STARS};
    for (size_t i = 0; i < sizeof(mechanisms) / sizeof(mechanisms[0]); ++i) {
        nb_peak_state_t p;
        nb_peak_core_init(&p);
        nb_peak_core_tick(&p, 1u, mechanisms[i]);
        CHECK(nb_peak_core_active_mechanism(&p) == mechanisms[i]);

        nb_peak_core_reset_transient(&p);
        CHECK(nb_peak_core_active_mechanism(&p) == NB_PEAK_MECHANISM_NONE);
        CHECK(nb_peak_core_fade_scale(&p) == 0.0f);
    }
}

/* Blink pausa só durante glifo de olho (HEART/TEARS/LAUGH), nunca durante
 * adorno. */
static void test_blink_pause_only_for_eye_glyphs(void)
{
    CHECK(nb_peak_core_is_eye_glyph(NB_PEAK_MECHANISM_HEART));
    CHECK(nb_peak_core_is_eye_glyph(NB_PEAK_MECHANISM_TEARS));
    CHECK(nb_peak_core_is_eye_glyph(NB_PEAK_MECHANISM_LAUGH));
    CHECK(!nb_peak_core_is_eye_glyph(NB_PEAK_MECHANISM_STARS));
    CHECK(!nb_peak_core_is_eye_glyph(NB_PEAK_MECHANISM_BLUSH));
    CHECK(!nb_peak_core_is_eye_glyph(NB_PEAK_MECHANISM_SWEAT_DROP));
    CHECK(!nb_peak_core_is_eye_glyph(NB_PEAK_MECHANISM_QUESTION));
    CHECK(!nb_peak_core_is_eye_glyph(NB_PEAK_MECHANISM_EXCLAMATION));
    CHECK(!nb_peak_core_is_eye_glyph(NB_PEAK_MECHANISM_ZZZ));
    CHECK(!nb_peak_core_is_eye_glyph(NB_PEAK_MECHANISM_NONE));

    nb_peak_state_t p;
    nb_peak_core_init(&p);
    nb_peak_core_tick(&p, 1u, NB_PEAK_MECHANISM_LAUGH);
    CHECK(nb_peak_core_blink_should_pause(&p));

    nb_peak_core_init(&p);
    nb_peak_core_tick(&p, 1u, NB_PEAK_MECHANISM_QUESTION);
    CHECK(!nb_peak_core_blink_should_pause(&p));
}

static void test_null_is_safe(void)
{
    nb_peak_core_init(NULL);
    nb_peak_core_tick(NULL, 10u, NB_PEAK_MECHANISM_HEART);
    nb_peak_core_reset_transient(NULL);
    CHECK(nb_peak_core_active_mechanism(NULL) == NB_PEAK_MECHANISM_NONE);
    CHECK(nb_peak_core_fade_scale(NULL) == 0.0f);
    CHECK(!nb_peak_core_blink_should_pause(NULL));
}

/* S3.8, item 7, emenda 2026-07-08: TEARS dispara exatamente 1x ao cruzar
 * >=0.70 com SAD dominante; retrigger com o vetor parado no fundo (sem
 * cair abaixo de 0.60) não deve acontecer. */
static void test_tears_fires_once_on_crossing_no_retrigger_at_bottom(void)
{
    nb_peak_tears_trigger_t t;
    nb_peak_tears_trigger_init(&t);

    CHECK(nb_peak_tears_trigger_tick(&t, true, 0.70f));
    for (int i = 0; i < 20; ++i) {
        CHECK(!nb_peak_tears_trigger_tick(&t, true, 0.90f)); /* vetor mora fundo -- sem retrigger */
    }
}

/* Rearma só quando a intensidade cai abaixo de 0.60 -- entre 0.60 e 0.70
 * ainda não rearma (histerese de verdade, não um limiar único). */
static void test_tears_rearms_only_below_exit_threshold(void)
{
    nb_peak_tears_trigger_t t;
    nb_peak_tears_trigger_init(&t);

    CHECK(nb_peak_tears_trigger_tick(&t, true, 0.70f));
    CHECK(!nb_peak_tears_trigger_tick(&t, true, 0.65f)); /* entre 0.60-0.70 -- ainda desarmado */
    CHECK(!nb_peak_tears_trigger_tick(&t, true, 0.70f));

    CHECK(!nb_peak_tears_trigger_tick(&t, true, 0.59f)); /* cruza o exit -- rearma, mas 0.59<0.70 não dispara */
    CHECK(nb_peak_tears_trigger_tick(&t, true, 0.70f));  /* agora sim, novo cruzamento */
}

/* Sem hub SAD dominante, nunca dispara, mesmo em intensidade alta. */
static void test_tears_never_fires_without_sad_dominant(void)
{
    nb_peak_tears_trigger_t t;
    nb_peak_tears_trigger_init(&t);
    for (int i = 0; i < 10; ++i) {
        CHECK(!nb_peak_tears_trigger_tick(&t, false, 0.95f));
    }
}

/* Abaixo do limiar de entrada, nunca dispara mesmo com SAD dominante. */
static void test_tears_never_fires_below_enter_threshold(void)
{
    nb_peak_tears_trigger_t t;
    nb_peak_tears_trigger_init(&t);
    for (int i = 0; i < 10; ++i) {
        CHECK(!nb_peak_tears_trigger_tick(&t, true, 0.69f));
    }
}

static void test_tears_null_is_safe(void)
{
    nb_peak_tears_trigger_init(NULL);
    CHECK(!nb_peak_tears_trigger_tick(NULL, true, 1.0f));
}

int main(void)
{
    test_init_is_none();
    test_no_request_never_activates();
    test_request_activates_immediately();
    test_never_two_peaks_simultaneously();
    test_expires_after_hold_even_if_still_requested();
    test_fade_envelope_shape();
    test_zzz_persists_without_timeout_or_fade();
    test_reset_transient_clears_any_mechanism();
    test_blink_pause_only_for_eye_glyphs();
    test_null_is_safe();
    test_tears_fires_once_on_crossing_no_retrigger_at_bottom();
    test_tears_rearms_only_below_exit_threshold();
    test_tears_never_fires_without_sad_dominant();
    test_tears_never_fires_below_enter_threshold();
    test_tears_null_is_safe();

    if (failures == 0) {
        printf("peak_core host_test: ok\n");
        return 0;
    }

    printf("peak_core host_test: %d failure(s)\n", failures);
    return 1;
}
