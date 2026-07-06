#ifndef NB_ENERGY_H
#define NB_ENERGY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro -- motor de energia da S3.7 completa (item 2 do plano
 * pós-go, docs/RFC-VIDA-V2.md §7, motor 3 "Energia"). Circadiano + tédio
 * + vetor modulam um nível contínuo de sonolência (`level`, [0,1]); quem
 * consome esse nível (idle_engine) desacelera blink/pálpebra a partir
 * dele. Sonolência não é motif: é o sistema desacelerando e afundando --
 * a decisão de cruzar pra SLEEPING continua sendo do `tiny_fsm`/
 * `circadian_core`, este núcleo só entrega o sinal contínuo.
 *
 * Determinístico e sem RNG (ao contrário de nb_attention/nb_posture --
 * energia não sorteia nada, só integra os três sinais de entrada).
 * Clock injetado via nb_energy_tick(dt_ms); tédio, ativação e quiet_mode
 * são injetados por quem chama (a casca, quando existir, alimenta a
 * partir de reflex_engine/emotion_core/circadian_core -- ver "Plano
 * S3.7 completo", item 3 "acoplamentos": esta fiação real ainda não
 * existe, só o núcleo).
 */

/* Tédio (ms sem estímulo acima de BASELINE/IDLE_MOTIF) atinge o teto de
 * sonolência depois desse tempo -- retunar em bancada quando a fiação
 * real existir. */
#define NB_ENERGY_BOREDOM_RAMP_MS 300000u /* 5 min */
/* Ativação alta (arousal > 0) empurra a sonolência pra baixo -- proporção
 * do quanto ela cancela o tédio acumulado. */
#define NB_ENERGY_AROUSAL_PULL 0.6f
/* Viés fixo somado quando quiet_mode está ligado (NIGHT/circadiano) --
 * mais sonolento à noite mesmo sem tédio acumulado. */
#define NB_ENERGY_QUIET_BIAS 0.3f
/* Suavização exponencial do nível resolvido rumo ao alvo -- sonolência
 * é um afundar gradual, não um degrau (mesma técnica de tau usada em
 * SOFT_DRIFT/nb_attention). */
#define NB_ENERGY_LEVEL_TAU_MS 5000.0f

typedef struct {
    float level; /* [0,1] -- 0 = alerta, 1 = teto de sonolência em IDLE */
} nb_energy_t;

void nb_energy_init(nb_energy_t *e);

/* Avança dt_ms com os três sinais de entrada deste instante:
 * - boredom_ms: ms desde o último estímulo acima de BASELINE/IDLE_MOTIF.
 * - arousal: [-1,1], vetor de ativação do emotion_core (só a componente
 *   positiva reduz sonolência -- tristeza/raiva não "acordam" o robô).
 * - quiet_mode: mesmo booleano já usado por nb_idle_engine_set_mode(). */
void nb_energy_tick(nb_energy_t *e, uint32_t dt_ms, uint32_t boredom_ms, float arousal,
                    bool quiet_mode);

#ifdef __cplusplus
}
#endif

#endif
