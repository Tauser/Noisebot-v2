#ifndef NB_RARITY_CORE_H
#define NB_RARITY_CORE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro das raridades (S3.8, item 8, RFC-VIDA-V2.md §10):
 * "SNEEZE (~1/dia), DREAM (<=1/noite, em SLEEPING), STARGAZE (<=1/noite,
 * NIGHT). Contadores por raridade em STATUS/dashboard."
 *
 * Emenda normativa (2026-07-08, decisão do usuário): gatilho é processo
 * de Poisson por hazard rate -- a cada tick em estado elegível,
 * p = dt_ms * taxa (memoryless, aproximação padrão pra p pequeno). O
 * núcleo agora decide QUANDO tentar (antes ficava pra casca, sem
 * gatilho definido); a casca só informa se o estado elegível vale neste
 * frame. Propriedade central: taxa nominal < teto pra DREAM/STARGAZE
 * (esperado por sessão fica abaixo do limite -- "algumas noites nada
 * acontece" é o comportamento típico, não a exceção); SNEEZE pode
 * saturar o teto de ~1/dia com mais frequência (taxa nominal 20h < teto
 * 24h), aceito pelo usuário como parte da decisão.
 *
 * SNEEZE: elegível = IDLE acordado (nunca quiet); taxa 1/~20h; teto
 * ~1/dia (intervalo mínimo desde o último disparo). DREAM: elegível =
 * SLEEPING; taxa 1/~24h de sono acumulado; teto 1/sessão contínua de
 * SLEEPING. STARGAZE: elegível = NIGHT + IDLE acordado; taxa 1/~4h;
 * teto 1/sessão contínua de NIGHT. "Sessão" não depende de relógio de
 * 24h literal (circadian_core roda acelerado em bancada, S3.4) -- é
 * delimitada pela própria fase/estado.
 *
 * Contadores mapeiam direto pros 3 campos de STATUS
 * (`rarity_sneeze_count`/`rarity_dream_count`/`rarity_stargaze_count`,
 * `protocol/nbp2.yaml`) -- envio de STATUS em si é pendência separada,
 * fora de escopo deste núcleo (ver ROADMAP.md).
 *
 * Sem FreeRTOS/ESP-IDF: tudo injetado.
 */

typedef enum {
    NB_RARITY_SNEEZE = 0,
    NB_RARITY_DREAM,
    NB_RARITY_STARGAZE,
    NB_RARITY_COUNT,
} nb_rarity_kind_t;

#define NB_RARITY_SNEEZE_MIN_INTERVAL_MS 86400000u /* ~1 dia, RFC "~1/dia" */

/* Taxas nominais (Poisson, eventos/ms) -- "1 a cada N horas", valores
 * práticos normativos (usuário, 2026-07-08), retunáveis em bancada. */
#define NB_RARITY_SNEEZE_RATE_PER_MS (1.0f / (20.0f * 3600000.0f))    /* 1/~20h */
#define NB_RARITY_DREAM_RATE_PER_MS (1.0f / (24.0f * 3600000.0f))     /* 1/~24h de sono */
#define NB_RARITY_STARGAZE_RATE_PER_MS (1.0f / (4.0f * 3600000.0f))   /* 1/~4h */

typedef struct {
    uint32_t rng_state;

    uint64_t last_sneeze_trigger_ms;
    bool has_last_sneeze_trigger;

    bool dream_available_this_session;
    bool was_sleeping;

    bool stargaze_available_this_session;
    bool was_night;

    uint16_t counts[NB_RARITY_COUNT]; /* desde o boot -- nunca resetam sozinhos */
} nb_rarity_state_t;

/* Semente 0 é substituída por um valor fixo não-zero (xorshift32 trava em
 * 0) -- mesma regra dos outros núcleos com RNG embutido. */
void nb_rarity_core_init(nb_rarity_state_t *state, uint32_t rng_seed);

/* Avança dt_ms; se is_eligible (IDLE acordado, nunca quiet) e ainda
 * dentro do teto de intervalo, rola o hazard rate de SNEEZE. now_ms é o
 * clock absoluto do chamador (só usado pro teto de intervalo mínimo).
 * Retorna true só no tick em que de fato disparou. */
bool nb_rarity_core_tick_sneeze(nb_rarity_state_t *state, uint32_t dt_ms, bool is_eligible,
                                uint64_t now_ms);

/* Avança dt_ms; is_sleeping é o estado atual (mesmo valor todo tick,
 * mesmo sem disparar -- necessário pra detectar a transição de sessão).
 * Enquanto SLEEPING e a sessão ainda não disparou, rola o hazard rate de
 * DREAM. */
bool nb_rarity_core_tick_dream(nb_rarity_state_t *state, uint32_t dt_ms, bool is_sleeping);

/* Mesma regra de nb_rarity_core_tick_dream, mas pra STARGAZE (is_night =
 * NIGHT + IDLE acordado, decidido pela casca). */
bool nb_rarity_core_tick_stargaze(nb_rarity_state_t *state, uint32_t dt_ms, bool is_night);

uint16_t nb_rarity_core_count(const nb_rarity_state_t *state, nb_rarity_kind_t kind);

#ifdef __cplusplus
}
#endif

#endif
