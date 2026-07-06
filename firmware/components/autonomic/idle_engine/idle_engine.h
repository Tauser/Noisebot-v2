#ifndef NB_IDLE_ENGINE_H
#define NB_IDLE_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

#include "nb_attention.h"
#include "nb_energy.h"
#include "nb_posture.h"

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

/* S3.7 -- spike go/no-go (passo 0, docs/RFC-VIDA-V2.md §7): liga o motor de
 * atenção (nb_attention.h) no lugar do SOFT_DRIFT/scheduling de motifs
 * longos, e a respiração (nb_breath.h) modulando a altura dos olhos. O
 * blink independente (Poisson) permanece intocado nas duas configs.
 * Ligada por padrão nesta fase (escopo do spike); comentar pra voltar ao
 * catálogo de motifs de S2.4 puro (usado pelos host-tests de regressão em
 * NB_IDLE_V2_SPIKE=0, docs/ROADMAP.md "Plano S3.7 -- passo 0"). */
#ifndef NB_IDLE_V2_SPIKE
#define NB_IDLE_V2_SPIKE 1
#endif

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
    /* S3.7 completo, item 4: gestos nomeados (RFC-VIDA-V2.md §7) -- só
     * agendados sob NB_IDLE_V2_SPIKE, mesmo slot exclusivo dos motifs
     * acima (nunca dois ao mesmo tempo). Fisiologia/automanutenção
     * (Regra da Causa, RFC §2), não "atenção" fingida. */
    NB_IDLE_MOTIF_CHECK_IN,   /* gaze à frente + micro-abertura + blink */
    NB_IDLE_MOTIF_SLOW_BLINK, /* blink lento (contentamento) */
    NB_IDLE_MOTIF_SIGH,       /* gaze desce e volta suave (acomodação) */
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

    /* S3.7 completo, item 3 (acoplamentos): fator de respiração deste
     * tick (mesmo valor aplicado a open_l/open_r sob NB_IDLE_V2_SPIKE) --
     * exposto pra casca sincronizar o LED idle em fase com a respiração
     * (RFC §7, "respiração ... em fase com o LED idle"). Sempre 1.0 em
     * !NB_IDLE_V2_SPIKE. */
    float breath_scale;
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

    /* S3.7 completo, item 4: gestos nomeados (só incrementam sob
     * NB_IDLE_V2_SPIKE). */
    uint32_t check_in_count;
    uint32_t slow_blink_count;
    uint32_t sigh_count;
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

    /* S3.7 spike: motor de atenção (nb_attention.h), alimenta drift_x/y
     * quando NB_IDLE_V2_SPIKE está ligado. Campo sempre presente (mesmo
     * com a flag desligada) pra manter o layout do struct estável entre
     * as duas configs de build. */
    nb_attention_t attention_v2;

    /* S3.7 completo, item 1 (nb_posture.h): motor de postura, soma-se ao
     * tilt/gaze/width quando NB_IDLE_V2_SPIKE está ligado. Mesma regra do
     * campo acima -- sempre presente, só usado sob a flag. */
    nb_posture_t posture_v2;

    /* S3.7 completo, item 2 (nb_energy.h): motor de energia (sonolência
     * contínua). Entradas injetadas pela casca via
     * nb_idle_engine_set_energy_inputs() -- sem chamada, ficam em 0
     * (nunca fica sonolento por padrão). Mesma regra dos campos acima. */
    nb_energy_t energy_v2;
    uint32_t energy_boredom_ms;
    float energy_arousal;

    /* S3.7 completo, item 3 (acoplamentos): "roll segue gaze com ~100ms
     * de atraso" (RFC §7) -- filtro passa-baixa do gaze horizontal,
     * somado ao tilt em compute_output() sob NB_IDLE_V2_SPIKE. */
    float roll_gaze_lag_x;

    /* S3.7 completo, item 4: agendamento independente dos 3 gestos
     * nomeados -- cada um no seu próprio timer, todos disputando o mesmo
     * slot de exclusividade (active_motif) que blink/motifs. */
    uint32_t next_check_in_at_ms;
    uint32_t next_slow_blink_at_ms;
    uint32_t next_sigh_at_ms;

    nb_idle_metrics_t metrics;
} nb_idle_engine_t;

/* Semente 0 é substituída por um valor fixo não-zero (xorshift32 trava em
 * 0). */
void nb_idle_engine_init(nb_idle_engine_t *engine, uint32_t rng_seed);

/* Entradas do motor de energia (nb_energy.h) -- tédio (ms desde o último
 * estímulo acima de BASELINE/IDLE_MOTIF) e ativação ([-1,1], vetor do
 * emotion_core). A casca chama isto a cada frame com os sinais reais;
 * sem chamada, o motor nunca fica sonolento (defaults 0/0.0). Gancho
 * exposto, ainda não ligado por nenhuma casca -- ver "Plano S3.7
 * completo", item 3 "acoplamentos". */
void nb_idle_engine_set_energy_inputs(nb_idle_engine_t *engine, uint32_t boredom_ms,
                                      float arousal);

/* Modo quiet (frequências ÷2, VISUAL.md §3) e atenção (IDLE: motifs longos
 * a cada 15-40s; ATTENTIVE: 5-13s). Não reagenda o motif em andamento. */
void nb_idle_engine_set_mode(nb_idle_engine_t *engine, bool quiet_mode,
                             nb_idle_attention_t attention);

/* Avança dt_ms e escreve a saída resolvida deste instante em out (pode ser
 * NULL se só as métricas interessarem). */
void nb_idle_engine_tick(nb_idle_engine_t *engine, uint32_t dt_ms, nb_idle_output_t *out);

const nb_idle_metrics_t *nb_idle_engine_get_metrics(const nb_idle_engine_t *engine);

/* Invariante H7 (ARCHITECTURE.md §4): zera o estado transitório dos
 * motores contínuos (postura, via nb_posture_reset_to_center(); e o lag
 * de roll-segue-gaze do item 3 "acoplamentos") ao entrar em IDLE -- a
 * deriva recomeça do centro dali. Gancho exposto, ainda não chamado por
 * nenhuma casca ("Plano S3.7 completo", item 3
 * "acoplamentos" liga isso a um evento real de tiny_fsm/main.c). */
void nb_idle_engine_reset_transient(nb_idle_engine_t *engine);

#ifdef __cplusplus
}
#endif

#endif
