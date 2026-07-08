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
 * Este núcleo só arbitra a TAXA (nunca mais que o teto) e o CONTADOR
 * (incrementa exatamente 1x por ocorrência de verdade); não decide QUANDO
 * tentar disparar cada raridade -- isso é narrativo/probabilístico e fica
 * pra casca (RFC não especifica gatilho, só o teto). Os contadores
 * mapeiam direto pros 3 campos novos de STATUS
 * (`rarity_sneeze_count`/`rarity_dream_count`/`rarity_stargaze_count`,
 * `protocol/nbp2.yaml`).
 *
 * SNEEZE não depende de estado -- limitado por intervalo mínimo desde o
 * último disparo (NB_RARITY_SNEEZE_MIN_INTERVAL_MS, ~1 dia, valor
 * prático). DREAM/STARGAZE são "por sessão": elegíveis de novo só quando
 * a condição de estado (SLEEPING/NIGHT) transiciona de fora pra dentro
 * de novo -- sem depender de um relógio de 24h literal (o clock do
 * circadian_core roda acelerado em bancada, S3.4; "uma noite" já é
 * delimitado pela própria fase, não por tempo de parede).
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

typedef struct {
    uint64_t last_sneeze_trigger_ms;
    bool has_last_sneeze_trigger;

    bool dream_available_this_session;
    bool was_sleeping;

    bool stargaze_available_this_session;
    bool was_night;

    uint16_t counts[NB_RARITY_COUNT]; /* desde o boot -- nunca resetam sozinhos */
} nb_rarity_state_t;

void nb_rarity_init(nb_rarity_state_t *state);

/* Tenta disparar SNEEZE agora (now_ms = clock absoluto do chamador).
 * Falha se ainda dentro de NB_RARITY_SNEEZE_MIN_INTERVAL_MS do último
 * disparo bem-sucedido. Incrementa o contador só em sucesso. */
bool nb_rarity_trigger_sneeze(nb_rarity_state_t *state, uint64_t now_ms);

/* Tenta disparar DREAM. is_sleeping é o estado atual (mesmo valor a cada
 * chamada, mesmo se não for tentar disparar agora -- necessário pra
 * detectar a transição de sessão). Falha se !is_sleeping, ou se já
 * disparou nesta sessão contínua de SLEEPING (rearma quando sai e volta a
 * entrar em SLEEPING). */
bool nb_rarity_trigger_dream(nb_rarity_state_t *state, bool is_sleeping);

/* Mesma regra de nb_rarity_trigger_dream, mas pra STARGAZE/NIGHT. */
bool nb_rarity_trigger_stargaze(nb_rarity_state_t *state, bool is_night);

uint16_t nb_rarity_count(const nb_rarity_state_t *state, nb_rarity_kind_t kind);

#ifdef __cplusplus
}
#endif

#endif
