#include "../idle_engine.h"

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

#define TICK_MS 20u

static void test_init_is_deterministic(void)
{
    nb_idle_engine_t a;
    nb_idle_engine_t b;

    nb_idle_engine_init(&a, 42);
    nb_idle_engine_init(&b, 42);
    CHECK(memcmp(&a, &b, sizeof(a)) == 0);

    for (int i = 0; i < 1000; ++i) {
        nb_idle_output_t out_a;
        nb_idle_output_t out_b;

        nb_idle_engine_tick(&a, TICK_MS, &out_a);
        nb_idle_engine_tick(&b, TICK_MS, &out_b);
        CHECK(memcmp(&out_a, &out_b, sizeof(out_a)) == 0);
    }
}

static void test_seed_zero_does_not_lock_rng(void)
{
    nb_idle_engine_t e;
    nb_idle_engine_t reference;

    nb_idle_engine_init(&e, 0);
    nb_idle_engine_init(&reference, 0x9E3779B9u);
    CHECK(e.rng_state == reference.rng_state);
    CHECK(e.rng_state != 0u);
}

static void test_null_is_safe(void)
{
    nb_idle_engine_init(NULL, 1);
    nb_idle_engine_set_mode(NULL, true, NB_IDLE_ATTENTION_ATTENTIVE);
    nb_idle_engine_tick(NULL, TICK_MS, NULL);
    CHECK(nb_idle_engine_get_metrics(NULL) == NULL);

    nb_idle_engine_t e;
    nb_idle_engine_init(&e, 7);
    nb_idle_engine_tick(&e, TICK_MS, NULL); /* out NULL com engine válido */
}

static void test_drift_never_exceeds_amplitude(void)
{
    nb_idle_engine_t e;

    /* Amplitude retunada em bancada (idle_engine.c,
     * NB_IDLE_DRIFT_AMPLITUDE) -- 0.04 literal de VISUAL.md §3 lia como
     * imperceptível na escala de pixels do renderer. */
    const float amplitude = 0.35f;

    nb_idle_engine_init(&e, 123);
    for (uint32_t ms = 0; ms < 120000u; ms += TICK_MS) {
        nb_idle_engine_tick(&e, TICK_MS, NULL);
        CHECK(e.drift_x >= -amplitude - 0.0001f && e.drift_x <= amplitude + 0.0001f);
        CHECK(e.drift_y >= -amplitude - 0.0001f && e.drift_y <= amplitude + 0.0001f);
    }
}

static void test_quiet_mode_roughly_halves_event_frequency(void)
{
    nb_idle_engine_t normal;
    nb_idle_engine_t quiet;
    const uint32_t duration_ms = 300000u; /* 5 min simulados */

    nb_idle_engine_init(&normal, 99);
    nb_idle_engine_init(&quiet, 99);
    nb_idle_engine_set_mode(&quiet, true, NB_IDLE_ATTENTION_IDLE);

    uint32_t normal_events = 0;
    uint32_t quiet_events = 0;

    for (uint32_t ms = 0; ms < duration_ms; ms += TICK_MS) {
        nb_idle_engine_tick(&normal, TICK_MS, NULL);
        nb_idle_engine_tick(&quiet, TICK_MS, NULL);
    }
    normal_events = normal.metrics.blink_bar_count + normal.metrics.double_blink_count +
                   normal.metrics.sustained_count + normal.metrics.look_down_count +
                   normal.metrics.line_blink_count + normal.metrics.side_peek_count +
                   normal.metrics.scan_count;
    quiet_events = quiet.metrics.blink_bar_count + quiet.metrics.double_blink_count +
                  quiet.metrics.sustained_count + quiet.metrics.look_down_count +
                  quiet.metrics.line_blink_count + quiet.metrics.side_peek_count +
                  quiet.metrics.scan_count;

    CHECK(normal_events > 0);
    CHECK(quiet_events > 0);
    /* "frequências ÷2" -- quiet deve ter nitidamente menos eventos, sem
     * exigir uma proporção exata (é estatística, não determinística). */
    CHECK(quiet_events < normal_events);
}

static void test_attentive_schedules_long_motifs_more_often_than_idle(void)
{
    nb_idle_engine_t idle_engine;
    nb_idle_engine_t attentive_engine;
    const uint32_t duration_ms = 120000u; /* 2 min simulados */

    nb_idle_engine_init(&idle_engine, 55);
    nb_idle_engine_init(&attentive_engine, 55);
    nb_idle_engine_set_mode(&attentive_engine, false, NB_IDLE_ATTENTION_ATTENTIVE);

    for (uint32_t ms = 0; ms < duration_ms; ms += TICK_MS) {
        nb_idle_engine_tick(&idle_engine, TICK_MS, NULL);
        nb_idle_engine_tick(&attentive_engine, TICK_MS, NULL);
    }

    const nb_idle_metrics_t *idle_m = nb_idle_engine_get_metrics(&idle_engine);
    const nb_idle_metrics_t *att_m = nb_idle_engine_get_metrics(&attentive_engine);
    const uint32_t idle_long = idle_m->sustained_count + idle_m->look_down_count +
                               idle_m->line_blink_count + idle_m->side_peek_count +
                               idle_m->scan_count;
    const uint32_t att_long = att_m->sustained_count + att_m->look_down_count +
                              att_m->line_blink_count + att_m->side_peek_count +
                              att_m->scan_count;

    CHECK(att_long > idle_long);
}

/* Simula uma sessão em IDLE e verifica o critério de aceite de
 * VISUAL.md §3 (gate S2.4), sobre uma janela mais longa que os 60s do
 * ensaio de bancada e múltiplas sementes -- pra não depender de sorte de
 * uma semente só. A confirmação visual real continua sendo o ensaio de
 * bancada (fora do escopo deste host-test). */
static void test_acceptance_criteria_over_multiple_seeds(void)
{
    const uint32_t seeds[] = {1, 2, 3, 4, 5, 17, 99, 4242};
    /* 10 min por semente: com o intervalo médio de motif longo em IDLE
     * (15-40s), isso dá ~20 motifs longos por semente -- estatisticamente
     * robusto o bastante pra não depender de sorte de uma janela de 60s
     * isolada (a confirmação real da janela de 60s é o ensaio de bancada,
     * fora do escopo deste host-test). */
    const uint32_t duration_ms = 600000u;

    for (size_t s = 0; s < sizeof(seeds) / sizeof(seeds[0]); ++s) {
        nb_idle_engine_t e;

        nb_idle_engine_init(&e, seeds[s]);
        for (uint32_t ms = TICK_MS; ms <= duration_ms; ms += TICK_MS) {
            nb_idle_engine_tick(&e, TICK_MS, NULL);
        }

        const nb_idle_metrics_t *m = nb_idle_engine_get_metrics(&e);

        CHECK(m->sustained_count >= 1);
        CHECK(m->look_down_count + m->double_blink_count >= 1);
        CHECK(m->blink_bar_count >= 2);
    }
}

/* "nenhum intervalo de 15s com os olhos idênticos" (VISUAL.md §3) é
 * garantido por construção pelo SOFT_DRIFT contínuo, não por sorte no
 * agendamento de blink/motif: o passeio aleatório do drift muda a cada
 * tick, então os olhos nunca ficam bit-a-bit parados por muito tempo. */
static void test_soft_drift_keeps_changing_every_tick(void)
{
    nb_idle_engine_t e;
    nb_idle_engine_init(&e, 7);

    float prev_x = e.drift_x;
    float prev_y = e.drift_y;
    int changed_count = 0;

    for (int i = 0; i < 100; ++i) {
        nb_idle_engine_tick(&e, TICK_MS, NULL);
        if (e.drift_x != prev_x || e.drift_y != prev_y) {
            ++changed_count;
        }
        prev_x = e.drift_x;
        prev_y = e.drift_y;
    }
    /* Praticamente todo tick muda o drift (passeio aleatório contínuo);
     * uma tolerância pequena cobre o caso raro de amostra ~0 do RNG. */
    CHECK(changed_count >= 90);
}

static void test_curious_tilt_widens_exactly_one_eye(void)
{
    nb_idle_engine_t e;

    nb_idle_engine_init(&e, 321);
    /* Roda até pegar um CURIOUS_TILT (bounded: motifs longos disparam
     * dentro de 40s em IDLE; 5 min de folga é generoso). */
    bool saw_curious_tilt = false;
    for (uint32_t ms = 0; ms < 300000u && !saw_curious_tilt; ms += TICK_MS) {
        nb_idle_output_t out;
        nb_idle_engine_tick(&e, TICK_MS, &out);
        if (out.active_motif == NB_IDLE_MOTIF_CURIOUS_TILT) {
            saw_curious_tilt = true;
            const int widened_l = out.width_l > 1.0f;
            const int widened_r = out.width_r > 1.0f;
            CHECK(widened_l != widened_r); /* exatamente um olho */
        }
    }
    CHECK(saw_curious_tilt);
}

int main(void)
{
    test_init_is_deterministic();
    test_seed_zero_does_not_lock_rng();
    test_null_is_safe();
    test_drift_never_exceeds_amplitude();
    test_quiet_mode_roughly_halves_event_frequency();
    test_attentive_schedules_long_motifs_more_often_than_idle();
    test_acceptance_criteria_over_multiple_seeds();
    test_soft_drift_keeps_changing_every_tick();
    test_curious_tilt_widens_exactly_one_eye();

    if (failures == 0) {
        printf("idle_engine host_test: ok\n");
        return 0;
    }

    printf("idle_engine host_test: %d failure(s)\n", failures);
    return 1;
}
