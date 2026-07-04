#ifndef NB_IDLE_ENGINE_H
#define NB_IDLE_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro do catálogo de motifs de IDLE (S2.4, VISUAL.md §3).
 *
 * Sem FreeRTOS/ESP-IDF: clock injetado via nb_idle_engine_tick(dt_ms) e RNG
 * embutido (xorshift32 determinístico, semente injetada no init) -- não usa
 * esp_random() pra ficar host-testável byte a byte. A casca (quando
 * existir) semeia com entropia real (esp_random()) e liga a saída ao
 * renderer (S2.2) e à tiny_fsm (S2.3, atenção IDLE×ATTENTIVE).
 *
 * SOFT_DRIFT é contínuo (sempre ativo, somado à saída). Blink (BLINK_BAR /
 * DOUBLE_BLINK / LINE_BLINK) segue um processo de Poisson próprio,
 * independente do agendamento dos motifs longos sustentados/peek/scan.
 */

typedef enum {
    NB_IDLE_MOTIF_NONE = 0,
    NB_IDLE_MOTIF_BLINK_BAR,
    NB_IDLE_MOTIF_DOUBLE_BLINK,
    NB_IDLE_MOTIF_CURIOUS_TILT,
    NB_IDLE_MOTIF_HEAD_TILT_HOLD,
    NB_IDLE_MOTIF_LOOK_DOWN_BLINK,
    NB_IDLE_MOTIF_LINE_BLINK,
    NB_IDLE_MOTIF_SIDE_PEEK,
    NB_IDLE_MOTIF_VERTICAL_SCAN,
    NB_IDLE_MOTIF_CROSS_SCAN,
    NB_IDLE_MOTIF_COUNT,
} nb_idle_motif_t;

typedef enum {
    NB_IDLE_ATTENTION_IDLE = 0,
    NB_IDLE_ATTENTION_ATTENTIVE = 1,
} nb_idle_attention_t;

/* Saída por tick -- o que o renderer (S2.2) precisaria somar ao estado
 * paramétrico da face. Nesta fatia só é consumida pelo host-test; a
 * integração real com o renderer fica para quando tiny_fsm (S2.3) alimentar
 * o modo de atenção via evento real. */
typedef struct {
    float gaze_x; /* [-1, 1], drift + motif */
    float gaze_y; /* [-1, 1] */
    float open_l; /* multiplicador de abertura, 1 = normal, 0 = fechado */
    float open_r;
    float width_l; /* multiplicador de largura, 1 = normal (CURIOUS_TILT: 1.3) */
    float width_r;
    float tilt;         /* [-1, 1]: assimetria de HEAD_TILT_HOLD */
    nb_idle_motif_t active_motif;
} nb_idle_output_t;

/* Métricas acumuladas -- usadas pelo host-test pra checar o critério de
 * aceite de VISUAL.md §3 (sessão de 60s) e pela casca pra log/telemetria. */
typedef struct {
    uint32_t blink_bar_count;
    uint32_t double_blink_count;
    uint32_t line_blink_count;
    uint32_t sustained_count; /* CURIOUS_TILT ou HEAD_TILT_HOLD */
    uint32_t look_down_count;
    uint32_t side_peek_count;
    uint32_t scan_count; /* VERTICAL_SCAN + CROSS_SCAN */
    uint32_t last_event_at_ms; /* último instante em que algo mudou (blink ou motif) */
} nb_idle_metrics_t;

typedef struct {
    uint32_t rng_state; /* xorshift32, nunca 0 */
    uint32_t now_ms;
    uint32_t next_blink_at_ms;
    uint32_t next_motif_at_ms;

    nb_idle_motif_t active_motif;
    uint32_t motif_started_ms;
    uint32_t motif_duration_ms;
    /* Sub-fase para motifs com sequência interna (DOUBLE_BLINK,
     * LOOK_DOWN_BLINK): 0 = fase única/inicial. */
    uint8_t motif_phase;

    bool quiet_mode;
    nb_idle_attention_t attention;

    float drift_x;
    float drift_y;
    float drift_target_x;
    float drift_target_y;
    uint32_t drift_next_target_at_ms;

    nb_idle_metrics_t metrics;
} nb_idle_engine_t;

/* Semente 0 é substituída por um valor fixo não-zero (xorshift32 trava em
 * 0). */
void nb_idle_engine_init(nb_idle_engine_t *engine, uint32_t rng_seed);

/* Modo quiet (frequências ÷2, VISUAL.md §3) e atenção (IDLE: motifs longos
 * a cada 15-40s; ATTENTIVE: 5-13s). Não reagenda o motif em andamento. */
void nb_idle_engine_set_mode(nb_idle_engine_t *engine, bool quiet_mode,
                             nb_idle_attention_t attention);

/* Avança dt_ms e escreve a saída resolvida deste instante em out (pode ser
 * NULL se só as métricas interessarem). */
void nb_idle_engine_tick(nb_idle_engine_t *engine, uint32_t dt_ms, nb_idle_output_t *out);

const nb_idle_metrics_t *nb_idle_engine_get_metrics(const nb_idle_engine_t *engine);

#ifdef __cplusplus
}
#endif

#endif
