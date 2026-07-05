#ifndef NB_CIRCADIAN_CORE_H
#define NB_CIRCADIAN_CORE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Núcleo C17 puro do relógio circadiano (S3.4, camada L4, BEHAVIOR.md §5).
 * Responsabilidade única (componente novo, não dentro de idle_engine --
 * decisão registrada em ROADMAP.md S3.4): resolve fase do dia (NIGHT/DAWN/
 * DAY/DUSK) a partir de uma âncora de tempo (unix_ms + instante do
 * relógio monotônico interno em que ela foi estabelecida), e deriva
 * brilho/quiet_mode dessa fase.
 *
 * Limiares de hora e níveis de brilho são #define tunáveis, travados por
 * host-test de regressão (mesma nota de tuning de reflex_engine/
 * led_service). A rampa contínua de DAWN/DUSK (não um crossfade à parte)
 * é o mecanismo de "entrada/saída suave" de SLEEPING exigido pelo gate:
 * o brilho já está no piso quando a casca dispara SLEEP/WAKE_HOUR.
 *
 * Sem FreeRTOS/ESP-IDF/NVS/TIME_SYNC: âncora e dt_ms injetados. Sem
 * âncora ainda, o default é neutro (DAY, brilho 1.0, has_time_source=
 * false) -- nunca força comportamento noturno antes de saber a hora de
 * verdade.
 */

#define NB_CIRCADIAN_NIGHT_START_HOUR 22u
#define NB_CIRCADIAN_DAWN_START_HOUR 6u
#define NB_CIRCADIAN_DAY_START_HOUR 8u
#define NB_CIRCADIAN_DUSK_START_HOUR 20u

#define NB_CIRCADIAN_NIGHT_BRIGHTNESS 0.05f
#define NB_CIRCADIAN_DAY_BRIGHTNESS 1.0f

typedef enum {
    NB_CIRCADIAN_PHASE_NIGHT = 0,
    NB_CIRCADIAN_PHASE_DAWN,
    NB_CIRCADIAN_PHASE_DAY,
    NB_CIRCADIAN_PHASE_DUSK,
    NB_CIRCADIAN_PHASE_COUNT,
} nb_circadian_phase_t;

typedef struct {
    nb_circadian_phase_t phase;
    bool quiet_mode;         /* true só em NIGHT (BEHAVIOR.md §5) */
    float brightness_scale;  /* [0,1] -- consumido por led_service */
    bool has_time_source;    /* false até o primeiro set_time_anchor */
} nb_circadian_output_t;

typedef struct {
    bool has_anchor;
    uint64_t anchor_unix_ms;
    uint64_t anchor_monotonic_ms; /* valor de monotonic_now_ms no instante da âncora */
    uint64_t monotonic_now_ms;    /* acumulado via tick(dt_ms), relativo ao init */
} nb_circadian_core_t;

void nb_circadian_core_init(nb_circadian_core_t *core);

/* Ancora o relógio: "agora" (no relógio monotônico interno) o unix_ms é
 * este valor. Chamar de novo recalibra (TIME_SYNC real ou fallback NVS). */
void nb_circadian_core_set_time_anchor(nb_circadian_core_t *core, uint64_t unix_ms);

/* Avança dt_ms e resolve a saída deste instante. */
nb_circadian_output_t nb_circadian_core_tick(nb_circadian_core_t *core, uint32_t dt_ms);

/* unix_ms estimado agora (âncora + decorrido), ou 0 sem âncora. */
uint64_t nb_circadian_core_now_unix_ms(const nb_circadian_core_t *core);

/* Exposto pra host-test/tunabilidade dos limiares. */
nb_circadian_phase_t nb_circadian_core_phase_for_hour(uint32_t hour_of_day);

#ifdef __cplusplus
}
#endif

#endif
