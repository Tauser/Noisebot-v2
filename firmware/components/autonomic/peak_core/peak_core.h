#ifndef NB_PEAK_CORE_H
#define NB_PEAK_CORE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro da régua de picos (S3.8, item 7, RFC-VIDA-V2.md §4):
 * glifos (olhos-pictograma, substituem o olho paramétrico) e adornos
 * (ficam junto da face, não substituem nada). "Regras anti-carnaval
 * (invioláveis)": só em pico (|vetor| > 0.7 ou momento de arco -- decidido
 * pela casca, este núcleo só recebe `requested`); máximo 1 mecanismo por
 * vez; 1-2s com fade (exceto Zzz); blink pausa durante glifo (não durante
 * adorno); entrada em IDLE limpa (H7).
 *
 * Este núcleo NÃO desenha nada nem sabe de emotion_core/arc_core/
 * touch_service -- só arbitra QUAL mecanismo está ativo agora e por
 * quanto tempo (exclusividade de slot único, mesmo padrão de
 * `active_motif` do idle_engine). A casca decide `requested` a cada tick
 * (pico do vetor em emotion_core, ou beat de um arco como o `♥` de
 * RECONCILE) e consome `nb_peak_core_active_mechanism()`/`fade_scale()`
 * pra desenhar via o pipeline SVG->PBM já existente (VISUAL.md §5).
 *
 * Sem FreeRTOS/ESP-IDF: clock injetado via nb_peak_core_tick(dt_ms).
 */

typedef enum {
    NB_PEAK_MECHANISM_NONE = 0,
    /* Glifos (olhos-pictograma) */
    NB_PEAK_MECHANISM_HEART, /* pico de CONTENT pós-carinho -- momento-assinatura */
    NB_PEAK_MECHANISM_TEARS, /* SAD profundo */
    NB_PEAK_MECHANISM_LAUGH, /* gargalhada, com boca aberta -- arco */
    /* @@ espiral (tontura) bloqueado por IMU -- fora de escopo (RFC §8) */

    /* Adornos (junto da face, não substituem o olho) */
    NB_PEAK_MECHANISM_STARS,      /* EUREKA/HAPPY extremo */
    NB_PEAK_MECHANISM_BLUSH,
    NB_PEAK_MECHANISM_SWEAT_DROP, /* CONFUSED */
    NB_PEAK_MECHANISM_QUESTION,
    NB_PEAK_MECHANISM_EXCLAMATION,
    NB_PEAK_MECHANISM_ZZZ,        /* SLEEPING -- único sem timeout/fade próprio */

    NB_PEAK_MECHANISM_COUNT,
} nb_peak_mechanism_t;

/* Duração padrão de um pico (RFC "1-2s com fade"); ZZZ ignora isto (RFC
 * "exceto Zzz" -- persiste enquanto for pedido, sem fade). Valor prático
 * único no meio da faixa -- retunar em bancada se algum mecanismo pedir
 * timing próprio. */
#define NB_PEAK_HOLD_MS 1500u
#define NB_PEAK_FADE_MS 300u /* borda de entrada/saída dentro do hold */

typedef struct {
    nb_peak_mechanism_t active;
    uint64_t now_ms;
    uint64_t active_since_ms;
} nb_peak_state_t;

void nb_peak_core_init(nb_peak_state_t *state);

/* Avança dt_ms. `requested` é o mecanismo que a casca quer ativo neste
 * frame (NONE = nenhum pedido). Exclusividade: só aceita um pedido novo
 * se o slot está livre (NONE ativo) -- "máximo 1 mecanismo por vez", o
 * primeiro a pedir vence, pedidos concorrentes de outros mecanismos são
 * ignorados até o slot liberar. ZZZ persiste enquanto `requested==ZZZ`
 * (sem timeout); os demais expiram sozinhos após NB_PEAK_HOLD_MS mesmo
 * que `requested` continue pedindo o mesmo mecanismo (pico é breve por
 * definição). */
void nb_peak_core_tick(nb_peak_state_t *state, uint32_t dt_ms, nb_peak_mechanism_t requested);

/* Entrada em IDLE do corpo -- limpa o slot imediatamente (H7), qualquer
 * que seja o mecanismo ativo. */
void nb_peak_core_reset_transient(nb_peak_state_t *state);

nb_peak_mechanism_t nb_peak_core_active_mechanism(const nb_peak_state_t *state);

/* Escala [0,1] do mecanismo ativo agora: sobe em NB_PEAK_FADE_MS, platô,
 * desce em NB_PEAK_FADE_MS antes do fim do hold. 0.0 se nada ativo.
 * Constante 1.0 pro ZZZ (RFC "exceto Zzz" -- sem fade). */
float nb_peak_core_fade_scale(const nb_peak_state_t *state);

/* true só pros 3 glifos de olho (HEART/TEARS/LAUGH) -- adornos não
 * ocupam o olho, não pausam blink (RFC "blink pausa durante glifo",
 * implicitamente não durante adorno). */
bool nb_peak_core_is_eye_glyph(nb_peak_mechanism_t mechanism);

bool nb_peak_core_blink_should_pause(const nb_peak_state_t *state);

#ifdef __cplusplus
}
#endif

#endif
