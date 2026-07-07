#include "idle_engine.h"

#include "nb_attention.h"
#include "nb_breath.h"
#include "nb_energy.h"
#include "nb_posture.h"

#include <math.h>
#include <string.h>

/* Evita depender de M_PI (macro POSIX/glibc de math.h, não garantida em
 * -std=c17 estrito -- mesma classe de problema do strnlen no protocolo
 * NBP/2, ver protocol/codegen/generate_nbp2.py). */
#define NB_IDLE_PI 3.14159265358979323846f

/* Efeito prático do motor de energia (nb_energy.h) sobre a saída, S3.7
 * completo item 2: no teto de sonolência (level=1), a pálpebra descansa
 * 25% mais fechada (nunca fecha de vez -- SLEEPING de verdade é outro
 * estado/visual) e o blink independente fica ~2x mais espaçado. */
#define NB_ENERGY_MAX_EYELID_DROOP 0.25f
#define NB_ENERGY_MAX_BLINK_SLOWDOWN 1.0f /* multiplicador extra no teto: 1x + 1x = 2x */

/* Acoplamentos (S3.7 completo, item 3, RFC-VIDA-V2.md §7): "roll segue
 * gaze com ~100ms de atraso" -- filtro passa-baixa do gaze horizontal
 * (tau abaixo), ganho pequeno pra ficar sutil (cabeça acompanha o olhar,
 * não copia o valor inteiro). */
#define NB_IDLE_ROLL_FOLLOWS_GAZE_TAU_MS 100.0f
#define NB_IDLE_ROLL_FOLLOWS_GAZE_GAIN 0.15f

/* Gestos nomeados (S3.7 completo, item 4, RFC-VIDA-V2.md §7). CHECK_IN
 * tem frequência literal do RFC (~1x/1-3min); SLOW_BLINK/SIGH não têm
 * número no RFC -- amplitudes/intervalos práticos, mesma classe de
 * retune-em-bancada já documentada no projeto. */
#define NB_IDLE_CHECK_IN_MIN_MS 60000u
#define NB_IDLE_CHECK_IN_MAX_MS 180000u
#define NB_IDLE_CHECK_IN_DURATION_MS 700u
#define NB_IDLE_CHECK_IN_OPEN_BOOST 0.08f /* "micro-abertura" */

#define NB_IDLE_SLOW_BLINK_MIN_MS 30000u
#define NB_IDLE_SLOW_BLINK_MAX_MS 90000u
#define NB_IDLE_SLOW_BLINK_DURATION_MS 650u /* bem mais lento que o blink normal (80-120ms) */

#define NB_IDLE_SIGH_MIN_MS 45000u
#define NB_IDLE_SIGH_MAX_MS 120000u
#define NB_IDLE_SIGH_DURATION_MS 1800u
#define NB_IDLE_SIGH_GAZE_DOWN_AMPLITUDE 0.15f /* bem mais sutil que o antigo LOOK_DOWN_BLINK (0.7) */

static uint32_t xorshift32(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/* [0, 1) com 24 bits de precisão -- suficiente pra timing/seleção de
 * gesto, não é criptográfico. */
static float rand01(uint32_t *state)
{
    return (float)(xorshift32(state) >> 8) / (float)(1u << 24);
}

static float clampf(float v, float lo, float hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

/* Curva S (3x²-2x³), velocidade zero nas duas pontas -- padrão pra
 * suavizar entrada/saída sem "pulo". x em [0,1]. */
static float smoothstep(float x)
{
    x = clampf(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

/* Envelope 0→1→0 com ataque/liberação suaves (smoothstep) e platô no meio
 * -- usado por CHECK_IN pra entrar e sair do pico sem "pular"
 * instantaneamente (bug real achado em bancada, S2.4: sem isso o olhar
 * parecia robótico, batendo na posição em vez de deslizar). */
static float ease_envelope(uint32_t elapsed, uint32_t duration, uint32_t attack_ms,
                           uint32_t release_ms)
{
    if (elapsed < attack_ms) {
        return smoothstep((float)elapsed / (float)attack_ms);
    }
    if (elapsed >= duration - release_ms) {
        return smoothstep((float)(duration - elapsed) / (float)release_ms);
    }
    return 1.0f;
}

static uint32_t sample_next_blink_ms(nb_idle_engine_t *e)
{
    /* Poisson via distribuição exponencial, piso de 1.8s (VISUAL.md §3).
     * Modo quiet: frequência ÷2 -> intervalo médio dobra. Motor de
     * energia (nb_energy.h, S3.7 completo item 2): sonolência espaça o
     * blink continuamente por cima disso. */
    const float energy_slowdown = 1.0f + e->energy_v2.level * NB_ENERGY_MAX_BLINK_SLOWDOWN;
    const float mean_ms = (e->quiet_mode ? 10000.0f : 5000.0f) * energy_slowdown;
    const float min_ms = (e->quiet_mode ? 3600.0f : 1800.0f) * energy_slowdown;
    float r = rand01(&e->rng_state);
    if (r < 0.0001f) {
        r = 0.0001f;
    }
    float interval = -mean_ms * logf(1.0f - r);
    if (interval < min_ms) {
        interval = min_ms;
    }
    return e->now_ms + (uint32_t)interval;
}

static void start_blink_motif(nb_idle_engine_t *e)
{
    const bool is_double = rand01(&e->rng_state) < 0.30f; /* ~30% dos blinks */

    e->motif_started_ms = e->now_ms;
    e->motif_phase = 0;
    if (is_double) {
        const uint32_t gap = 400u + (uint32_t)(rand01(&e->rng_state) * 600.0f);
        e->active_motif = NB_IDLE_MOTIF_DOUBLE_BLINK;
        e->motif_duration_ms = 100u + gap + 100u;
        e->metrics.double_blink_count++;
    } else {
        e->active_motif = NB_IDLE_MOTIF_BLINK_BAR;
        e->motif_duration_ms = 80u + (uint32_t)(rand01(&e->rng_state) * 40.0f);
        e->metrics.blink_bar_count++;
    }
    e->metrics.last_event_at_ms = e->now_ms;
    e->next_blink_at_ms = sample_next_blink_ms(e);
}

/* Acoplamento blink×sacada (RFC-VIDA-V2.md §7, "Plano S3.7 completo" item
 * 3): dispara um blink de fato quando uma sacada começa, respeitando a
 * exclusividade de slot (não interrompe um motif já em andamento).
 * start_blink_motif() já resorteia next_blink_at_ms no final -- o blink
 * independente (Poisson) e o disparado por sacada usam o mesmo
 * agendador, "fundidos" sem precisar de um mecanismo novo. */
static void on_attention_saccade(void *ctx)
{
    nb_idle_engine_t *e = (nb_idle_engine_t *)ctx;

    if (e->active_motif == NB_IDLE_MOTIF_NONE) {
        start_blink_motif(e);
    }
}

/* Gestos nomeados (S3.7 completo, item 4): agendamento independente por
 * gesto, cada um com seu próprio intervalo -- quiet_mode dobra os três,
 * mesma regra do blink. */
static uint32_t sample_next_check_in_ms(nb_idle_engine_t *e)
{
    const uint32_t lo = e->quiet_mode ? NB_IDLE_CHECK_IN_MIN_MS * 2u : NB_IDLE_CHECK_IN_MIN_MS;
    const uint32_t span = NB_IDLE_CHECK_IN_MAX_MS - NB_IDLE_CHECK_IN_MIN_MS;
    return e->now_ms + lo + (uint32_t)(rand01(&e->rng_state) * (float)span);
}

static uint32_t sample_next_slow_blink_ms(nb_idle_engine_t *e)
{
    const uint32_t lo =
        e->quiet_mode ? NB_IDLE_SLOW_BLINK_MIN_MS * 2u : NB_IDLE_SLOW_BLINK_MIN_MS;
    const uint32_t span = NB_IDLE_SLOW_BLINK_MAX_MS - NB_IDLE_SLOW_BLINK_MIN_MS;
    return e->now_ms + lo + (uint32_t)(rand01(&e->rng_state) * (float)span);
}

static uint32_t sample_next_sigh_ms(nb_idle_engine_t *e)
{
    const uint32_t lo = e->quiet_mode ? NB_IDLE_SIGH_MIN_MS * 2u : NB_IDLE_SIGH_MIN_MS;
    const uint32_t span = NB_IDLE_SIGH_MAX_MS - NB_IDLE_SIGH_MIN_MS;
    return e->now_ms + lo + (uint32_t)(rand01(&e->rng_state) * (float)span);
}

static void start_check_in(nb_idle_engine_t *e)
{
    e->active_motif = NB_IDLE_MOTIF_CHECK_IN;
    e->motif_started_ms = e->now_ms;
    e->motif_duration_ms = NB_IDLE_CHECK_IN_DURATION_MS;
    e->metrics.check_in_count++;
    e->metrics.last_event_at_ms = e->now_ms;
    e->next_check_in_at_ms = sample_next_check_in_ms(e);
}

static void start_slow_blink(nb_idle_engine_t *e)
{
    e->active_motif = NB_IDLE_MOTIF_SLOW_BLINK;
    e->motif_started_ms = e->now_ms;
    e->motif_duration_ms = NB_IDLE_SLOW_BLINK_DURATION_MS;
    e->metrics.slow_blink_count++;
    e->metrics.last_event_at_ms = e->now_ms;
    e->next_slow_blink_at_ms = sample_next_slow_blink_ms(e);
}

static void start_sigh(nb_idle_engine_t *e)
{
    e->active_motif = NB_IDLE_MOTIF_SIGH;
    e->motif_started_ms = e->now_ms;
    e->motif_duration_ms = NB_IDLE_SIGH_DURATION_MS;
    e->metrics.sigh_count++;
    e->metrics.last_event_at_ms = e->now_ms;
    e->next_sigh_at_ms = sample_next_sigh_ms(e);
}

static void compute_output(const nb_idle_engine_t *e, nb_idle_output_t *out)
{
    if (out == NULL) {
        return;
    }

    out->gaze_x = e->drift_x;
    out->gaze_y = e->drift_y;
    out->open_l = 1.0f;
    out->open_r = 1.0f;
    out->width_l = 1.0f;
    out->width_r = 1.0f;
    out->tilt = 0.0f;
    out->active_motif = e->active_motif;

    const uint32_t elapsed = e->now_ms - e->motif_started_ms;

    switch (e->active_motif) {
    case NB_IDLE_MOTIF_BLINK_BAR:
        out->open_l = 0.0f;
        out->open_r = 0.0f;
        break;

    case NB_IDLE_MOTIF_DOUBLE_BLINK:
        if (elapsed < 100u || elapsed >= e->motif_duration_ms - 100u) {
            out->open_l = 0.0f;
            out->open_r = 0.0f;
        }
        break;

    case NB_IDLE_MOTIF_CHECK_IN: {
        /* "gaze à frente + micro-abertura + blink" (RFC §7): puxa o gaze
         * (o que quer que a atenção/postura estejam pedindo) de volta pro
         * centro durante o pico do gesto, abre um pouco mais os olhos
         * (env positivo), e fecha rápido perto do fim -- ease_envelope dá
         * o "sobe, segura, desce" em vez de saltar. */
        const float env = ease_envelope(elapsed, e->motif_duration_ms, 200u, 200u);

        out->gaze_x *= (1.0f - env);
        out->gaze_y *= (1.0f - env);
        out->open_l *= 1.0f + NB_IDLE_CHECK_IN_OPEN_BOOST * env;
        out->open_r *= 1.0f + NB_IDLE_CHECK_IN_OPEN_BOOST * env;
        if (elapsed >= e->motif_duration_ms - 150u && elapsed < e->motif_duration_ms - 50u) {
            out->open_l = 0.0f;
            out->open_r = 0.0f;
        }
        break;
    }

    case NB_IDLE_MOTIF_SLOW_BLINK: {
        /* Blink lento (contentamento): fecha e reabre suavemente (onda
         * senoidal, não corte instantâneo do blink normal). */
        const float t = (float)elapsed / (float)e->motif_duration_ms;
        const float close_amount = sinf(NB_IDLE_PI * t);

        out->open_l *= 1.0f - close_amount;
        out->open_r *= 1.0f - close_amount;
        break;
    }

    case NB_IDLE_MOTIF_SIGH: {
        /* Acomodação: gaze desce e volta suave, bem mais sutil que o
         * antigo LOOK_DOWN_BLINK (sem blink, sem hold -- só um
         * "afundar"). */
        const float t = (float)elapsed / (float)e->motif_duration_ms;

        out->gaze_y += NB_IDLE_SIGH_GAZE_DOWN_AMPLITUDE * sinf(NB_IDLE_PI * t);
        break;
    }

    case NB_IDLE_MOTIF_NONE:
    default:
        break;
    }

    /* Respiração (RFC-VIDA-V2.md §7): modula a altura/abertura dos olhos
     * continuamente. Aplicada depois do blink -- 0 * fator continua 0, o
     * olho fechado não "respira". */
    const float breath = nb_breath_scale(e->now_ms, NB_BREATH_PERIOD_MS_DEFAULT,
                                         NB_BREATH_AMPLITUDE_DEFAULT);
    out->open_l *= breath;
    out->open_r *= breath;
    out->breath_scale = breath;

    /* Energia (nb_energy.h, S3.7 completo item 2): pálpebra descansa mais
     * fechada conforme a sonolência sobe -- multiplicativo, então o blink
     * (0) continua fechando de vez independente do nível. */
    const float eyelid_droop = 1.0f - e->energy_v2.level * NB_ENERGY_MAX_EYELID_DROOP;
    out->open_l *= eyelid_droop;
    out->open_r *= eyelid_droop;

    /* Postura (RFC-VIDA-V2.md §7, "Plano S3.7 completo" item 1): soma-se
     * ao gaze/tilt/width já resolvidos -- não substitui a atenção nem os
     * motifs, é a deriva lenta do baseline por baixo de tudo. */
    out->gaze_x += e->posture_v2.gaze_x;
    out->gaze_y += e->posture_v2.gaze_y;
    out->tilt += e->posture_v2.roll;
    out->width_l += e->posture_v2.asymmetry;
    out->width_r -= e->posture_v2.asymmetry;

    /* "Roll segue gaze com ~100ms de atraso" (RFC §7, item 3): soma um
     * ganho pequeno do gaze horizontal já suavizado (nb_idle_engine_tick
     * atualiza roll_gaze_lag_x). */
    out->tilt += e->roll_gaze_lag_x * NB_IDLE_ROLL_FOLLOWS_GAZE_GAIN;
}

void nb_idle_engine_init(nb_idle_engine_t *engine, uint32_t rng_seed)
{
    if (engine == NULL) {
        return;
    }
    memset(engine, 0, sizeof(*engine));
    engine->rng_state = (rng_seed != 0u) ? rng_seed : 0x9E3779B9u;
    engine->attention = NB_IDLE_ATTENTION_IDLE;
    engine->next_blink_at_ms = sample_next_blink_ms(engine);
    /* Sementes derivadas, cada uma distinta -- xorshift32 não pode
     * receber o mesmo estado inicial de engine->rng_state sem acoplar as
     * sequências de sorteio dos diferentes motores. */
    nb_attention_init(&engine->attention_v2, engine->rng_state ^ 0x1234567u);
    /* Blink×sacada (S3.7 completo, item 3). */
    nb_attention_set_saccade_callback(&engine->attention_v2, on_attention_saccade, engine);
    nb_posture_init(&engine->posture_v2, engine->rng_state ^ 0x87654321u);
    nb_energy_init(&engine->energy_v2); /* sem RNG -- não precisa de semente */
    /* Gestos nomeados (item 4). */
    engine->next_check_in_at_ms = sample_next_check_in_ms(engine);
    engine->next_slow_blink_at_ms = sample_next_slow_blink_ms(engine);
    engine->next_sigh_at_ms = sample_next_sigh_ms(engine);
}

void nb_idle_engine_reset_transient(nb_idle_engine_t *engine)
{
    if (engine == NULL) {
        return;
    }
    nb_posture_reset_to_center(&engine->posture_v2);
    /* Roll-segue-gaze (item 3, "acoplamentos") também é estado
     * transitório -- reseta junto pra não carregar residual de fora de
     * IDLE pra dentro (mesma disciplina do H7). */
    engine->roll_gaze_lag_x = 0.0f;
}

void nb_idle_engine_set_mode(nb_idle_engine_t *engine, bool quiet_mode,
                             nb_idle_attention_t attention)
{
    if (engine == NULL) {
        return;
    }
    engine->quiet_mode = quiet_mode;
    engine->attention = attention;
}

void nb_idle_engine_set_energy_inputs(nb_idle_engine_t *engine, uint32_t boredom_ms,
                                      float arousal)
{
    if (engine == NULL) {
        return;
    }
    engine->energy_boredom_ms = boredom_ms;
    engine->energy_arousal = arousal;
}

void nb_idle_engine_tick(nb_idle_engine_t *engine, uint32_t dt_ms, nb_idle_output_t *out)
{
    if (engine == NULL) {
        return;
    }
    engine->now_ms += dt_ms;

    /* Motor de atenção (RFC-VIDA-V2.md §7) resolve o gaze: fixação+sacada
     * com viés de retorno ao centro. Escreve direto em drift_x/y --
     * compute_output() só lê o valor já resolvido. */
    nb_attention_tick(&engine->attention_v2, dt_ms, &engine->drift_x, &engine->drift_y);
    /* Postura (RFC-VIDA-V2.md §7, motor 2): tickada incondicionalmente
     * junto da atenção -- a contribuição é somada em compute_output(). */
    nb_posture_tick(&engine->posture_v2, dt_ms);
    /* Energia (RFC-VIDA-V2.md §7, motor 3): entradas vêm de
     * nb_idle_engine_set_energy_inputs() (default 0/0.0 sem casca real
     * chamando ainda). quiet_mode é o mesmo campo já usado acima. */
    nb_energy_tick(&engine->energy_v2, dt_ms, engine->energy_boredom_ms, engine->energy_arousal,
                  engine->quiet_mode);
    /* "Roll segue gaze com ~100ms de atraso" (RFC §7, item 3): filtro
     * passa-baixa do gaze horizontal já resolvido acima. */
    {
        const float roll_lag_alpha =
            clampf((float)dt_ms / NB_IDLE_ROLL_FOLLOWS_GAZE_TAU_MS, 0.0f, 1.0f);
        engine->roll_gaze_lag_x +=
            (engine->drift_x - engine->roll_gaze_lag_x) * roll_lag_alpha;
    }

    if (engine->active_motif != NB_IDLE_MOTIF_NONE &&
        engine->now_ms - engine->motif_started_ms >= engine->motif_duration_ms) {
        engine->active_motif = NB_IDLE_MOTIF_NONE;
    }

    /* Blink independente e gestos nomeados só disparam com o slot livre
     * -- nenhum interrompe um motif em andamento. Checados em sequência
     * -- se dois calharem de vencer no mesmo tick (raro, timers bem
     * espaçados), o primeiro checado ganha o slot livre. */
    if (engine->active_motif == NB_IDLE_MOTIF_NONE && engine->now_ms >= engine->next_blink_at_ms) {
        start_blink_motif(engine);
    } else if (engine->active_motif == NB_IDLE_MOTIF_NONE &&
              engine->now_ms >= engine->next_check_in_at_ms) {
        start_check_in(engine);
    } else if (engine->active_motif == NB_IDLE_MOTIF_NONE &&
              engine->now_ms >= engine->next_slow_blink_at_ms) {
        start_slow_blink(engine);
    } else if (engine->active_motif == NB_IDLE_MOTIF_NONE &&
              engine->now_ms >= engine->next_sigh_at_ms) {
        start_sigh(engine);
    }

    compute_output(engine, out);
}

const nb_idle_metrics_t *nb_idle_engine_get_metrics(const nb_idle_engine_t *engine)
{
    return (engine != NULL) ? &engine->metrics : NULL;
}
