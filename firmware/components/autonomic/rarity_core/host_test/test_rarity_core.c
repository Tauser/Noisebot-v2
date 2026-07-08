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
    nb_rarity_init(&r);
    CHECK(nb_rarity_count(&r, NB_RARITY_SNEEZE) == 0u);
    CHECK(nb_rarity_count(&r, NB_RARITY_DREAM) == 0u);
    CHECK(nb_rarity_count(&r, NB_RARITY_STARGAZE) == 0u);
}

/* SNEEZE: primeiro disparo sempre permitido; segundo dentro do intervalo
 * mínimo é recusado; depois do intervalo, permitido de novo. Contador
 * incrementa exatamente 1x por sucesso. */
static void test_sneeze_rate_limited_and_counted(void)
{
    nb_rarity_state_t r;
    nb_rarity_init(&r);

    CHECK(nb_rarity_trigger_sneeze(&r, 0u));
    CHECK(nb_rarity_count(&r, NB_RARITY_SNEEZE) == 1u);

    CHECK(!nb_rarity_trigger_sneeze(&r, 1000u));
    CHECK(!nb_rarity_trigger_sneeze(&r, NB_RARITY_SNEEZE_MIN_INTERVAL_MS - 1u));
    CHECK(nb_rarity_count(&r, NB_RARITY_SNEEZE) == 1u); /* tentativas falhas não incrementam */

    CHECK(nb_rarity_trigger_sneeze(&r, NB_RARITY_SNEEZE_MIN_INTERVAL_MS));
    CHECK(nb_rarity_count(&r, NB_RARITY_SNEEZE) == 2u);
}

/* DREAM: nunca dispara fora de SLEEPING. */
static void test_dream_requires_sleeping(void)
{
    nb_rarity_state_t r;
    nb_rarity_init(&r);
    CHECK(!nb_rarity_trigger_dream(&r, false));
    CHECK(nb_rarity_count(&r, NB_RARITY_DREAM) == 0u);
}

/* DREAM: no máximo 1 por sessão contínua de SLEEPING, mesmo com muitas
 * tentativas dentro da mesma sessão. */
static void test_dream_at_most_once_per_sleep_session(void)
{
    nb_rarity_state_t r;
    nb_rarity_init(&r);

    CHECK(nb_rarity_trigger_dream(&r, true));
    for (int i = 0; i < 50; ++i) {
        CHECK(!nb_rarity_trigger_dream(&r, true));
    }
    CHECK(nb_rarity_count(&r, NB_RARITY_DREAM) == 1u);
}

/* DREAM: sair de SLEEPING e voltar rearma pra uma nova sessão. */
static void test_dream_rearms_after_leaving_and_reentering_sleep(void)
{
    nb_rarity_state_t r;
    nb_rarity_init(&r);

    CHECK(nb_rarity_trigger_dream(&r, true));
    CHECK(!nb_rarity_trigger_dream(&r, true));

    CHECK(!nb_rarity_trigger_dream(&r, false)); /* saiu de SLEEPING */
    CHECK(!nb_rarity_trigger_dream(&r, false));

    CHECK(nb_rarity_trigger_dream(&r, true)); /* nova sessão -- elegível de novo */
    CHECK(nb_rarity_count(&r, NB_RARITY_DREAM) == 2u);
}

/* STARGAZE: mesmas regras de DREAM, gate é NIGHT em vez de SLEEPING --
 * núcleos independentes, um não interfere no outro. */
static void test_stargaze_same_session_rules_independent_of_dream(void)
{
    nb_rarity_state_t r;
    nb_rarity_init(&r);

    CHECK(!nb_rarity_trigger_stargaze(&r, false));
    CHECK(nb_rarity_trigger_stargaze(&r, true));
    CHECK(!nb_rarity_trigger_stargaze(&r, true));
    CHECK(nb_rarity_count(&r, NB_RARITY_STARGAZE) == 1u);

    /* DREAM continua com seu próprio estado, intocado. */
    CHECK(nb_rarity_count(&r, NB_RARITY_DREAM) == 0u);
    CHECK(nb_rarity_trigger_dream(&r, true));
    CHECK(nb_rarity_count(&r, NB_RARITY_DREAM) == 1u);
    CHECK(nb_rarity_count(&r, NB_RARITY_STARGAZE) == 1u); /* stargaze não mudou */

    CHECK(!nb_rarity_trigger_stargaze(&r, false));
    CHECK(nb_rarity_trigger_stargaze(&r, true));
    CHECK(nb_rarity_count(&r, NB_RARITY_STARGAZE) == 2u);
}

static void test_null_is_safe(void)
{
    nb_rarity_init(NULL);
    CHECK(!nb_rarity_trigger_sneeze(NULL, 0u));
    CHECK(!nb_rarity_trigger_dream(NULL, true));
    CHECK(!nb_rarity_trigger_stargaze(NULL, true));
    CHECK(nb_rarity_count(NULL, NB_RARITY_SNEEZE) == 0u);
}

static void test_count_out_of_range_is_safe(void)
{
    nb_rarity_state_t r;
    nb_rarity_init(&r);
    CHECK(nb_rarity_count(&r, NB_RARITY_COUNT) == 0u);
}

int main(void)
{
    test_init_counts_are_zero();
    test_sneeze_rate_limited_and_counted();
    test_dream_requires_sleeping();
    test_dream_at_most_once_per_sleep_session();
    test_dream_rearms_after_leaving_and_reentering_sleep();
    test_stargaze_same_session_rules_independent_of_dream();
    test_null_is_safe();
    test_count_out_of_range_is_safe();

    if (failures == 0) {
        printf("rarity_core host_test: ok\n");
        return 0;
    }

    printf("rarity_core host_test: %d failure(s)\n", failures);
    return 1;
}
