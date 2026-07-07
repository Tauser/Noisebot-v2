#ifndef NB_EMOTION_CORE_H
#define NB_EMOTION_CORE_H

#include <stdbool.h>

#include "renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro do modelo emocional v0 (S2.5, BEHAVIOR.md §2).
 *
 * Vetor contínuo 2D (valência × ativação), clamp em ±1. Estímulos somam
 * deltas (clampados); decaimento exponencial para (0,0) com constante tal
 * que o pico cai a <5% em ~60s (BEHAVIOR.md §2). O vetor mapeia por
 * nearest-neighbor às âncoras das 10 expressões-base (VISUAL.md §2, mesmo
 * enum `nb_face_expr_t` do renderer -- S2.5 é L4, chama L3 adjacente,
 * `ARCHITECTURE.md` §2, sem duplicar a tabela de expressões).
 *
 * Sem FreeRTOS/ESP-IDF/NVS: clock injetado via nb_emotion_core_tick(dt_ms).
 * Persistência da última expressão (BEHAVIOR.md §2 "Persistência") fica
 * para a casca, quando existir -- fora do escopo desta fatia.
 */

/* Variante episódica (S3.7 completo, item 7, RFC-VIDA-V2.md §3.1): duas
 * por hub, sorteada ao entrar na região (troca de hub dominante), dura o
 * episódio inteiro. */
typedef enum {
    NB_EMOTION_VARIANT_A = 0,
    NB_EMOTION_VARIANT_B = 1,
} nb_emotion_variant_t;

typedef struct {
    float valence; /* [-1, +1] */
    float arousal; /* [-1, +1] */

    /* S3.7 completo, item 6 (RFC-VIDA-V2.md §7 "circadiano no vetor"):
     * offset de ativação alimentado pela casca (NIGHT -0.15 a -0.25, DAWN
     * +0.10 por ~30min) -- entrada por parâmetro em vez de emotion_core
     * ler circadian_core diretamente, mesma lição do item 2 (energia).
     * Gancho exposto via nb_emotion_core_set_circadian_offset(), ainda
     * não ligado por nenhuma casca (default 0.0, sem efeito). */
    float circadian_arousal_offset;

    /* S3.7 completo, item 7: RNG embutido (xorshift32, mesma técnica dos
     * núcleos do idle_engine) só pra sortear a variante episódica --
     * nada mais aqui é estocástico. hub dominante + variante ativa
     * rastreados pra detectar troca de episódio em
     * nb_emotion_core_resolve_face(). */
    uint32_t rng_state;
    nb_face_expr_t dominant_hub;
    nb_emotion_variant_t active_variant;
    bool has_dominant_hub; /* false até a primeira resolve_face() */
} nb_emotion_state_t;

/* Semente 0 é substituída por um valor fixo não-zero (xorshift32 trava em
 * 0) -- mesma regra dos núcleos do idle_engine. */
void nb_emotion_core_init(nb_emotion_state_t *state, uint32_t rng_seed);

/* Soma um estímulo {delta_valence, delta_arousal} ao vetor, clampando o
 * resultado em [-1, +1] em cada eixo (BEHAVIOR.md §2: "delta é clampeado
 * por tick"). */
void nb_emotion_core_apply_stimulus(nb_emotion_state_t *state, float delta_valence,
                                    float delta_arousal);

/* Offset circadiano de ativação (RFC §7) -- somado ao alvo de decay do
 * eixo de ativação em nb_emotion_core_tick(). A casca chama isto a cada
 * frame com o sinal real de circadian_core; sem chamada, fica em 0.0
 * (sem efeito). */
void nb_emotion_core_set_circadian_offset(nb_emotion_state_t *state, float arousal_offset);

/* Avança dt_ms com decaimento exponencial rumo ao temperamento
 * (+0.10, +0.05 -- RFC §7: "em paz, o Noise é sutilmente caloroso e
 * desperto") somado ao offset circadiano no eixo de ativação, em vez de
 * decair pra (0,0). */
void nb_emotion_core_tick(nb_emotion_state_t *state, uint32_t dt_ms);

/* Expressão-âncora mais próxima do vetor atual (distância euclidiana no
 * plano valência×ativação, VISUAL.md §2). Nunca falha -- sempre retorna
 * uma das NB_FACE_EXPR_COUNT expressões. Mantida por compatibilidade;
 * S3.7 completo (item 6) resolve a face via nb_emotion_core_resolve_face()
 * em vez de nearest-neighbor + transição de 220ms. */
nb_face_expr_t nb_emotion_core_nearest_expression(const nb_emotion_state_t *state);

/* Campo contínuo (S3.7 completo, item 6, RFC-VIDA-V2.md §3): resolve o
 * vetor (v,a) diretamente num nb_face_state_t, por blend contínuo entre
 * os 4 hubs (NEUTRAL/HAPPY/SAD/ANGRY -- únicos com boca/campo contínuo;
 * as outras 6 âncoras saem de uso nesta decisão de produto, 2026-07-06).
 * Pesos por distância inversa ao quadrado (Shepard), normalizados; se o
 * vetor coincide exatamente com uma âncora, retorna aquela âncora +
 * variante (sem divisão por zero). Nunca falha -- out sempre escrito.
 *
 * S3.7 completo, item 7 (variantes episódicas, RFC §3.1): detecta troca
 * de hub dominante (o de maior peso) e sorteia uma nova variante (A/B,
 * 50/50) só nesse instante -- state deixa de ser const porque isso muda
 * rng_state/dominant_hub/active_variant. A variante tempera o resultado
 * do blend (escalada pelo peso do hub dominante, então esmaece junto com
 * ele -- nunca um salto visual na troca de episódio). */
void nb_emotion_core_resolve_face(nb_emotion_state_t *state, nb_face_state_t *out);

float nb_emotion_core_clampf(float v, float lo, float hi);

#ifdef __cplusplus
}
#endif

#endif
