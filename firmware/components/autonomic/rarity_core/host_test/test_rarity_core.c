#include "../rarity_core.h"

#include <stdio.h>

static int failures;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);           \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static void test_init_counts_are_zero(void)
{
    nb_rarity_state_t r;
    nb_rarity_core_init(&r, 1u);
    CHECK(nb_rarity_core_count(&r, NB_RARITY_SNEEZE) == 0u);
    CHECK(nb_rarity_core_count(&r, NB_RARITY_DREAM) == 0u);
    CHECK(nb_rarity_core_count(&r, NB_RARITY_STARGAZE) == 0u);
}

/* Nenhuma raridade dispara fora do estado elegível, por mais ticks que
 * passem. */
static void test_nothing_fires_outside_eligible_state(void)
{
    nb_rarity_state_t r;
    nb_rarity_core_init(&r, 1u);
    for (int i = 0; i < 100000; ++i) {
        CHECK(!nb_rarity_core_tick_sneeze(&r, 60000u, false, (uint64_t)i * 60000u));
        CHECK(!nb_rarity_core_tick_dream(&r, 60000u, false));
        CHECK(!nb_rarity_core_tick_stargaze(&r, 60000u, false));
    }
    CHECK(nb_rarity_core_count(&r, NB_RARITY_SNEEZE) == 0u);
    CHECK(nb_rarity_core_count(&r, NB_RARITY_DREAM) == 0u);
    CHECK(nb_rarity_core_count(&r, NB_RARITY_STARGAZE) == 0u);
}

/* Taxa empírica de SNEEZE (elegível o tempo todo, sem o teto de
 * intervalo atrapalhar -- roda por só ~5h simulados, bem abaixo do teto
 * de 1 dia) fica dentro de +-30% da nominal (1/20h) sob clock acelerado
 * (ticks de 1 min, muitas repetições). */
static void test_sneeze_empirical_rate_within_tolerance(void)
{
    const uint32_t dt_ms = 60000u; /* 1 min por tick */
    const uint32_t ticks = 300u;   /* 5h simulados, bem abaixo do teto de 1 dia */
    const float expected_per_run = (float)(dt_ms * ticks) * NB_RARITY_SNEEZE_RATE_PER_MS;
    const uint32_t runs = 20000u;
    uint32_t fires_total = 0u;

    /* Único estado/stream de RNG contínuo ao longo de todas as
     * "corridas" -- xorshift32 com seeds sequenciais pequenas (1,2,3...)
     * tem correlação conhecida entre streams curtos recém-semeados;
     * reseedar a cada corrida (descartado num teste anterior) enviesou o
     * resultado. Um stream só, bem misturado, evita isso.
     *
     * Reseta só o rastreamento do teto (has_last_sneeze_trigger) entre
     * corridas, não o RNG -- sem isso, rodar 5h por corrida sem pausa
     * faz o teto de 1 dia interagir com a taxa (média de 20h < teto de
     * 24h -- o próprio comportamento aceito pelo usuário), medindo a
     * taxa efetiva pós-teto em vez da taxa crua do hazard rate que este
     * teste quer provar isoladamente. */
    nb_rarity_state_t r;
    nb_rarity_core_init(&r, 12345u);
    uint64_t now_ms = 0u;

    for (uint32_t run = 0; run < runs; ++run) {
        r.has_last_sneeze_trigger = false;
        for (uint32_t i = 0; i < ticks; ++i) {
            now_ms += dt_ms;
            if (nb_rarity_core_tick_sneeze(&r, dt_ms, true, now_ms)) {
                ++fires_total;
            }
        }
    }

    const float empirical = (float)fires_total / (float)runs;
    CHECK(empirical > expected_per_run * 0.7f && empirical < expected_per_run * 1.3f);
}

/* Mesma prova pra STARGAZE (taxa 1/~4h), sessões curtas (~10min) onde o
 * teto de 1/sessão quase nunca bate, então a taxa empírica de fato
 * reflete a taxa nominal -- não o teto. */
static void test_stargaze_empirical_rate_within_tolerance_short_session(void)
{
    const uint32_t dt_ms = 60000u;
    const uint32_t ticks = 10u; /* sessão de NIGHT de 10 min */
    const float expected_per_session = (float)(dt_ms * ticks) * NB_RARITY_STARGAZE_RATE_PER_MS;
    const uint32_t sessions = 20000u;
    uint32_t fires_total = 0u;

    /* Único estado/stream contínuo entre sessões (mesma razão do teste
     * de SNEEZE acima) -- um tick com is_night=false entre sessões só
     * rearma o slot, sem reseedar o RNG. */
    nb_rarity_state_t r;
    nb_rarity_core_init(&r, 67890u);

    for (uint32_t s = 0; s < sessions; ++s) {
        (void)nb_rarity_core_tick_stargaze(&r, dt_ms, false);
        for (uint32_t i = 0; i < ticks; ++i) {
            if (nb_rarity_core_tick_stargaze(&r, dt_ms, true)) {
                ++fires_total;
            }
        }
    }

    const float empirical = (float)fires_total / (float)sessions;
    CHECK(empirical > expected_per_session * 0.7f && empirical < expected_per_session * 1.3f);
}

/* Teto: SNEEZE nunca dispara 2x dentro do intervalo mínimo, mesmo com a
 * taxa nominal favorecendo disparo mais cedo (20h < teto de 24h). */
static void test_sneeze_never_exceeds_cap(void)
{
    nb_rarity_state_t r;
    nb_rarity_core_init(&r, 7u);
    uint64_t now_ms = 0u;
    const uint32_t dt_ms = 60000u;
    uint64_t last_fire_ms = 0u;
    bool has_last_fire = false;

    for (uint32_t i = 0; i < 5000u; ++i) { /* ~208 dias simulados */
        now_ms += dt_ms;
        if (nb_rarity_core_tick_sneeze(&r, dt_ms, true, now_ms)) {
            if (has_last_fire) {
                CHECK(now_ms - last_fire_ms >= NB_RARITY_SNEEZE_MIN_INTERVAL_MS);
            }
            last_fire_ms = now_ms;
            has_last_fire = true;
        }
    }
}

/* DREAM: no máximo 1 por sessão contínua de SLEEPING, mesmo rodando a
 * sessão por muito mais tempo que o período nominal (24h) -- a taxa
 * garante que pode disparar, o teto garante que não dispara 2x. */
static void test_dream_at_most_once_per_sleep_session(void)
{
    nb_rarity_state_t r;
    nb_rarity_core_init(&r, 3u);
    const uint32_t dt_ms = 3600000u; /* 1h por tick */
    uint32_t fires = 0u;
    for (uint32_t i = 0; i < 200u; ++i) { /* 200h simulados, bem acima da taxa nominal */
        if (nb_rarity_core_tick_dream(&r, dt_ms, true)) {
            ++fires;
        }
    }
    CHECK(fires <= 1u);
    CHECK(nb_rarity_core_count(&r, NB_RARITY_DREAM) == fires);
}

/* DREAM: sair de SLEEPING e voltar rearma pra uma nova sessão. */
static void test_dream_rearms_after_leaving_and_reentering_sleep(void)
{
    nb_rarity_state_t r;
    nb_rarity_core_init(&r, 3u);
    const uint32_t dt_ms = 3600000u;

    uint32_t fires_session1 = 0u;
    for (uint32_t i = 0; i < 200u; ++i) {
        if (nb_rarity_core_tick_dream(&r, dt_ms, true)) {
            ++fires_session1;
        }
    }
    CHECK(fires_session1 <= 1u);

    CHECK(!nb_rarity_core_tick_dream(&r, dt_ms, false)); /* saiu de SLEEPING */

    uint32_t fires_session2 = 0u;
    for (uint32_t i = 0; i < 200u; ++i) {
        if (nb_rarity_core_tick_dream(&r, dt_ms, true)) {
            ++fires_session2;
        }
    }
    CHECK(fires_session2 <= 1u);
    CHECK(nb_rarity_core_count(&r, NB_RARITY_DREAM) == fires_session1 + fires_session2);
}

/* STARGAZE: mesmas regras de sessão, independente de DREAM (núcleos de
 * estado separados dentro do mesmo struct). */
static void test_stargaze_independent_of_dream(void)
{
    nb_rarity_state_t r;
    nb_rarity_core_init(&r, 5u);
    const uint32_t dt_ms = 3600000u;

    uint32_t stargaze_fires = 0u;
    for (uint32_t i = 0; i < 200u; ++i) {
        if (nb_rarity_core_tick_stargaze(&r, dt_ms, true)) {
            ++stargaze_fires;
        }
    }
    CHECK(stargaze_fires <= 1u);
    CHECK(nb_rarity_core_count(&r, NB_RARITY_DREAM) == 0u); /* dream nunca chamado -- intocado */
}

static void test_null_is_safe(void)
{
    nb_rarity_core_init(NULL, 1u);
    CHECK(!nb_rarity_core_tick_sneeze(NULL, 1000u, true, 0u));
    CHECK(!nb_rarity_core_tick_dream(NULL, 1000u, true));
    CHECK(!nb_rarity_core_tick_stargaze(NULL, 1000u, true));
    CHECK(nb_rarity_core_count(NULL, NB_RARITY_SNEEZE) == 0u);
}

static void test_count_out_of_range_is_safe(void)
{
    nb_rarity_state_t r;
    nb_rarity_core_init(&r, 1u);
    CHECK(nb_rarity_core_count(&r, NB_RARITY_COUNT) == 0u);
}

int main(void)
{
    test_init_counts_are_zero();
    test_nothing_fires_outside_eligible_state();
    test_sneeze_empirical_rate_within_tolerance();
    test_stargaze_empirical_rate_within_tolerance_short_session();
    test_sneeze_never_exceeds_cap();
    test_dream_at_most_once_per_sleep_session();
    test_dream_rearms_after_leaving_and_reentering_sleep();
    test_stargaze_independent_of_dream();
    test_null_is_safe();
    test_count_out_of_range_is_safe();

    if (failures == 0) {
        printf("rarity_core host_test: ok\n");
        return 0;
    }

    printf("rarity_core host_test: %d failure(s)\n", failures);
    return 1;
}
