#ifndef NB_SEARCH_H
#define NB_SEARCH_H

#include <stdbool.h>
#include <stdint.h>

#include "arc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro do arco SEARCH (S3.8, item 6, RFC-VIDA-V2.md §5.2):
 * "orientação (~300ms, perk + roll segue gaze) -> procura (2-4 vistadas
 * sacádicas, padrões LATERAL/DIAGONAL/ORBIT) -> desfecho obrigatório
 * (achou: perk + tilt + micro-HAPPY; não achou: blink lento + SIGH).
 * Causas: estímulo, ou tédio longo em companion (<=1/h)."
 *
 * "Padrões... extensão do motor de atenção do S3.7, não mecanismo novo"
 * (plano S3.8) -- este núcleo só decide QUAL padrão/quantas vistadas/se
 * achou, não calcula ângulos de gaze (isso é trabalho de
 * idle_engine/nb_attention na casca, fora de escopo aqui). RNG próprio
 * (xorshift32, mesma técnica de emotion_core/idle_engine) decide padrão
 * (nunca repete o anterior), contagem de vistadas (2-4) e o desfecho
 * achou/não-achou (NB2 não tem câmera -- é floreio comportamental, não
 * detecção real; probabilidade e durações são valores práticos, retunar
 * em bancada).
 *
 * Compõe nb_arc_state_t (arc_core, item 3): ORIENT=300ms, EXECUTE=
 * glance_count*NB_SEARCH_GLANCE_MS, OUTCOME=achou (500ms) ou não-achou
 * (650ms blink lento + 1800ms SIGH, sub-beats dentro da mesma fase).
 *
 * Dois gatilhos com regras diferentes (RFC): estímulo direto sem limite
 * de taxa próprio (só a exclusividade normal de arc_core -- não pode
 * disparar duas vezes rodando ao mesmo tempo); tédio limitado a <=1/h,
 * rastreado neste núcleo (não usa o cooldown_ms de arc_core, que fica
 * 0 -- um limite geral bloquearia também o gatilho de estímulo, que o
 * RFC não restringe).
 *
 * Sem FreeRTOS/ESP-IDF: tudo injetado.
 */

#define NB_SEARCH_ORIENT_MS 300u
#define NB_SEARCH_GLANCE_MS 600u
#define NB_SEARCH_GLANCE_MIN_COUNT 2u
#define NB_SEARCH_GLANCE_MAX_COUNT 4u
#define NB_SEARCH_OUTCOME_FOUND_MS 500u
#define NB_SEARCH_NOT_FOUND_BLINK_MS 650u
#define NB_SEARCH_NOT_FOUND_SIGH_MS 1800u
#define NB_SEARCH_FOUND_PROBABILITY 0.5f
#define NB_SEARCH_BOREDOM_MIN_INTERVAL_MS 3600000u /* 1h, RFC "<=1/h" */

typedef enum {
    NB_SEARCH_PATTERN_LATERAL = 0,
    NB_SEARCH_PATTERN_DIAGONAL,
    NB_SEARCH_PATTERN_ORBIT,
    NB_SEARCH_PATTERN_COUNT, /* = "nenhum padrão ativo" quando usado como valor */
} nb_search_pattern_t;

typedef enum {
    NB_SEARCH_BEAT_NONE = 0,
    NB_SEARCH_BEAT_ORIENT,
    NB_SEARCH_BEAT_SEARCHING,
    NB_SEARCH_BEAT_FOUND,
    NB_SEARCH_BEAT_NOT_FOUND_BLINK,
    NB_SEARCH_BEAT_NOT_FOUND_SIGH,
} nb_search_beat_t;

typedef struct {
    nb_arc_state_t arc;
    uint32_t rng_state;
    nb_search_pattern_t last_pattern;
    bool has_last_pattern;
    uint32_t last_glance_count;
    bool outcome_found;
    uint64_t last_boredom_trigger_ms;
    bool has_last_boredom_trigger;
} nb_search_state_t;

/* Semente 0 é substituída por um valor fixo não-zero (xorshift32 trava em
 * 0) -- mesma regra dos outros núcleos com RNG embutido. */
void nb_search_init(nb_search_state_t *state, uint32_t rng_seed);

/* Gatilho por estímulo direto -- sem limite de taxa próprio. Falha (false)
 * só se o arco já está rodando. */
bool nb_search_trigger_stimulus(nb_search_state_t *state);

/* Gatilho por tédio longo em modo companion -- limitado a
 * NB_SEARCH_BOREDOM_MIN_INTERVAL_MS desde o último disparo por tédio
 * bem-sucedido (now_ms é o clock absoluto do chamador, mesmo padrão de
 * nb_reflex_engine_on_stimulus). Falha se ainda dentro do intervalo, ou
 * se o arco já está rodando. */
bool nb_search_trigger_boredom(nb_search_state_t *state, uint64_t now_ms);

void nb_search_tick(nb_search_state_t *state, uint32_t dt_ms);

/* Entrada em IDLE do corpo -- aborta em qualquer fase. */
void nb_search_abort(nb_search_state_t *state);

nb_search_beat_t nb_search_current_beat(const nb_search_state_t *state);

/* Padrão da corrida atual (ou da última, mesmo após terminar --
 * NB_SEARCH_PATTERN_COUNT antes do primeiro disparo). */
nb_search_pattern_t nb_search_current_pattern(const nb_search_state_t *state);

/* Contagem de vistadas sorteada pra corrida atual/última (2-4, 0 antes do
 * primeiro disparo) -- exposta pra bancada/tuning, não consumida pelo
 * próprio núcleo além de calcular a duração do EXECUTE. */
uint32_t nb_search_current_glance_count(const nb_search_state_t *state);

#ifdef __cplusplus
}
#endif

#endif
