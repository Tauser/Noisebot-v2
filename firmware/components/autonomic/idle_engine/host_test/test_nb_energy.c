#include "../nb_energy.h"

#include <stdio.h>

static int failures;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);           \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

#define TICK_MS 1000u /* boredom_ms cresce em passos grandes -- não precisa de 20ms aqui */

static void test_init_is_zero(void)
{
    nb_energy_t e;

    nb_energy_init(&e);
    CHECK(e.level == 0.0f);
}

static void test_null_is_safe(void)
{
    nb_energy_init(NULL);
    nb_energy_tick(NULL, TICK_MS, 0u, 0.0f, false);

    nb_energy_t e;
    nb_energy_init(&e);
    nb_energy_tick(&e, TICK_MS, 0u, 0.0f, false); /* struct válida, sem NULL */
}

static void test_deterministic(void)
{
    nb_energy_t a;
    nb_energy_t b;

    nb_energy_init(&a);
    nb_energy_init(&b);
    for (uint32_t ms = 0; ms < 600000u; ms += TICK_MS) {
        nb_energy_tick(&a, TICK_MS, ms, 0.0f, false);
        nb_energy_tick(&b, TICK_MS, ms, 0.0f, false);
        CHECK(a.level == b.level);
    }
}

static void test_level_never_leaves_unit_interval(void)
{
    nb_energy_t e;

    nb_energy_init(&e);
    for (uint32_t ms = 0; ms < 3600000u; ms += TICK_MS) { /* 1h simulada */
        /* Entradas extremas de propósito -- tédio enorme, quiet ligado. */
        nb_energy_tick(&e, TICK_MS, ms * 10u, -1.0f, true);
        CHECK(e.level >= -0.0001f && e.level <= 1.0001f);
    }
}

/* Tédio crescente sem estímulo faz o nível subir -- "sistema
 * desacelerando" do RFC §7. */
static void test_level_rises_with_boredom(void)
{
    nb_energy_t e;

    nb_energy_init(&e);
    for (uint32_t ms = 0; ms < NB_ENERGY_BOREDOM_RAMP_MS * 2u; ms += TICK_MS) {
        nb_energy_tick(&e, TICK_MS, ms, 0.0f, false);
    }
    CHECK(e.level > 0.9f); /* tédio no teto por muito mais que o tempo de rampa */
}

/* Ativação alta (arousal positivo) reduz a sonolência mesmo com tédio
 * acumulado -- vetor "acorda" o robô. */
static void test_high_arousal_reduces_level_vs_baseline(void)
{
    nb_energy_t bored;
    nb_energy_t bored_and_aroused;

    nb_energy_init(&bored);
    nb_energy_init(&bored_and_aroused);
    for (uint32_t ms = 0; ms < NB_ENERGY_BOREDOM_RAMP_MS * 2u; ms += TICK_MS) {
        nb_energy_tick(&bored, TICK_MS, ms, 0.0f, false);
        nb_energy_tick(&bored_and_aroused, TICK_MS, ms, 1.0f, false);
    }
    CHECK(bored_and_aroused.level < bored.level);
}

/* quiet_mode (circadiano/NIGHT) adiciona sonolência de base mesmo sem
 * tédio acumulado. */
static void test_quiet_mode_raises_baseline_level(void)
{
    nb_energy_t normal;
    nb_energy_t quiet;

    nb_energy_init(&normal);
    nb_energy_init(&quiet);
    for (uint32_t ms = 0; ms < 60000u; ms += TICK_MS) { /* pouco tédio acumulado */
        nb_energy_tick(&normal, TICK_MS, 0u, 0.0f, false);
        nb_energy_tick(&quiet, TICK_MS, 0u, 0.0f, true);
    }
    CHECK(quiet.level > normal.level);
}

/* Suavização: o nível não pula em degrau de um tick pro outro mesmo
 * quando o alvo muda abruptamente (transição alerta -> tédio máximo). */
static void test_level_changes_gradually_not_in_steps(void)
{
    nb_energy_t e;

    nb_energy_init(&e);
    /* alvo passa de ~0 (tédio zero) pra 1 (tédio máximo) de um tick pro
     * outro -- o nível resolvido não deve acompanhar instantaneamente. */
    nb_energy_tick(&e, TICK_MS, 0u, 0.0f, false);
    const float before = e.level;
    nb_energy_tick(&e, TICK_MS, NB_ENERGY_BOREDOM_RAMP_MS * 10u, 0.0f, false);
    CHECK(e.level - before < 0.5f); /* movimento parcial, não salto pro alvo */
}

int main(void)
{
    test_init_is_zero();
    test_null_is_safe();
    test_deterministic();
    test_level_never_leaves_unit_interval();
    test_level_rises_with_boredom();
    test_high_arousal_reduces_level_vs_baseline();
    test_quiet_mode_raises_baseline_level();
    test_level_changes_gradually_not_in_steps();

    if (failures == 0) {
        printf("nb_energy host_test: ok\n");
        return 0;
    }

    printf("nb_energy host_test: %d failure(s)\n", failures);
    return 1;
}
