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

/* Amplitude prática de SOFT_DRIFT e dos motifs de gaze, calibrada em
 * bancada (2026-07-04) -- os valores literais de VISUAL.md §3 (drift
 * ≤0.04, side-peek/look-down 0.3-0.4) mapeiam pra menos de 1px até ~12px
 * na escala de pixels do renderer (kGazeXTravel=14, kYTravel=32,
 * S2.2) e ficam imperceptíveis ou parecem um "flick" curto demais.
 * Retunado observando o robô ao vivo; mantém a intenção de
 * VISUAL.md (drift sutil, motifs claramente vivos), não os números
 * literais do documento. */
#define NB_IDLE_DRIFT_AMPLITUDE 0.35f
#define NB_IDLE_SIDE_PEEK_AMPLITUDE 0.6f
#define NB_IDLE_LOOK_DOWN_AMPLITUDE 0.7f
#define NB_IDLE_SCAN_AMPLITUDE 0.7f

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

static uint32_t xorshift32(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/* [0, 1) com 24 bits de precisão -- suficiente pra timing/seleção de motif,
 * não é criptográfico. */
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
 * -- usado por SIDE_PEEK/LOOK_DOWN_BLINK pra entrar e sair do alvo sem
 * "pular" instantaneamente (bug real achado em bancada: sem isso o olhar
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
     * blink continuamente por cima disso -- em legado (!NB_IDLE_V2_SPIKE)
     * energy_v2.level nunca sai de 0 (nunca tickado), então o fator abaixo
     * é sempre 1.0 e não muda o comportamento existente. */
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

/* Sorteio de motifs longos e sua distribuição só existem na config sem o
 * spike (NB_IDLE_V2_SPIKE=0) -- sob a flag, o motor de atenção e a
 * respiração tomam o lugar deles (docs/ROADMAP.md "Plano S3.7 -- passo
 * 0"). Guardado atrás de #if pra não violar -Werror=unused-function. */
#if !NB_IDLE_V2_SPIKE
static uint32_t sample_next_motif_ms(nb_idle_engine_t *e)
{
    uint32_t lo = (e->attention == NB_IDLE_ATTENTION_ATTENTIVE) ? 5000u : 15000u;
    uint32_t hi = (e->attention == NB_IDLE_ATTENTION_ATTENTIVE) ? 13000u : 40000u;
    if (e->quiet_mode) {
        lo *= 2u;
        hi *= 2u;
    }
    const uint32_t span = hi - lo;
    const uint32_t offset = (uint32_t)(rand01(&e->rng_state) * (float)span);
    return e->now_ms + lo + offset;
}

/* Distribuição dos motifs longos (VISUAL.md §3): CURIOUS_TILT 30%,
 * HEAD_TILT_HOLD 20%, LOOK_DOWN_BLINK 15%, LINE_BLINK 15%, SIDE_PEEK 10%,
 * VERTICAL_SCAN 5%, CROSS_SCAN 5%. */
static nb_idle_motif_t pick_long_motif(nb_idle_engine_t *e)
{
    float r = rand01(&e->rng_state) * 100.0f;

    if (r < 30.0f) {
        return NB_IDLE_MOTIF_CURIOUS_TILT;
    }
    r -= 30.0f;
    if (r < 20.0f) {
        return NB_IDLE_MOTIF_HEAD_TILT_HOLD;
    }
    r -= 20.0f;
    if (r < 15.0f) {
        return NB_IDLE_MOTIF_LOOK_DOWN_BLINK;
    }
    r -= 15.0f;
    if (r < 15.0f) {
        return NB_IDLE_MOTIF_LINE_BLINK;
    }
    r -= 15.0f;
    if (r < 10.0f) {
        return NB_IDLE_MOTIF_SIDE_PEEK;
    }
    r -= 10.0f;
    if (r < 5.0f) {
        return NB_IDLE_MOTIF_VERTICAL_SCAN;
    }
    return NB_IDLE_MOTIF_CROSS_SCAN;
}

#endif /* !NB_IDLE_V2_SPIKE */

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

#if NB_IDLE_V2_SPIKE
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
#endif

#if !NB_IDLE_V2_SPIKE
static void start_long_motif(nb_idle_engine_t *e)
{
    const nb_idle_motif_t motif = pick_long_motif(e);

    e->active_motif = motif;
    e->motif_started_ms = e->now_ms;
    /* Lado (esquerdo/direito) aleatório para CURIOUS_TILT/SIDE_PEEK. */
    e->motif_phase = (rand01(&e->rng_state) < 0.5f) ? 0u : 1u;

    switch (motif) {
    case NB_IDLE_MOTIF_CURIOUS_TILT:
        e->motif_duration_ms = 3000u + (uint32_t)(rand01(&e->rng_state) * 2000.0f);
        e->metrics.sustained_count++;
        break;
    case NB_IDLE_MOTIF_HEAD_TILT_HOLD:
        e->motif_duration_ms = 5000u + (uint32_t)(rand01(&e->rng_state) * 10000.0f);
        e->metrics.sustained_count++;
        break;
    case NB_IDLE_MOTIF_LOOK_DOWN_BLINK:
        /* Duração maior que VISUAL.md §3 (~2s) para caber o ataque/
         * liberação suaves (500/900ms) sem virar um "flick" curto demais
         * -- retunado em bancada, ver NB_IDLE_LOOK_DOWN_AMPLITUDE acima. */
        e->motif_duration_ms = 3200u;
        e->metrics.look_down_count++;
        break;
    case NB_IDLE_MOTIF_LINE_BLINK:
        e->motif_duration_ms = 100u;
        e->metrics.line_blink_count++;
        break;
    case NB_IDLE_MOTIF_SIDE_PEEK:
        e->motif_duration_ms = 2500u;
        e->metrics.side_peek_count++;
        break;
    case NB_IDLE_MOTIF_VERTICAL_SCAN:
    case NB_IDLE_MOTIF_CROSS_SCAN:
        e->motif_duration_ms = 3500u;
        e->metrics.scan_count++;
        break;
    default:
        break;
    }
    e->metrics.last_event_at_ms = e->now_ms;
    e->next_motif_at_ms = sample_next_motif_ms(e);
}
#endif /* !NB_IDLE_V2_SPIKE */

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
    out->breath_scale = 1.0f; /* sobrescrito abaixo sob NB_IDLE_V2_SPIKE */

    const uint32_t elapsed = e->now_ms - e->motif_started_ms;

    switch (e->active_motif) {
    case NB_IDLE_MOTIF_BLINK_BAR:
    case NB_IDLE_MOTIF_LINE_BLINK:
        out->open_l = 0.0f;
        out->open_r = 0.0f;
        break;

    case NB_IDLE_MOTIF_DOUBLE_BLINK:
        if (elapsed < 100u || elapsed >= e->motif_duration_ms - 100u) {
            out->open_l = 0.0f;
            out->open_r = 0.0f;
        }
        break;

    case NB_IDLE_MOTIF_CURIOUS_TILT:
        /* "blink preserva forma" -- não zera open_l/r mesmo se um blink
         * independente calhar de rodar (o slot é exclusivo, então isso só
         * importaria se removêssemos essa exclusividade no futuro). */
        if (e->motif_phase == 0u) {
            out->width_l = 1.3f;
        } else {
            out->width_r = 1.3f;
        }
        break;

    case NB_IDLE_MOTIF_HEAD_TILT_HOLD:
        out->tilt = (e->motif_phase == 0u) ? 0.10f : -0.10f;
        break;

    case NB_IDLE_MOTIF_LOOK_DOWN_BLINK: {
        /* Ataque/liberação suaves (500ms/900ms) em vez de saltar direto
         * pro alvo -- ver ease_envelope(). Blinks acontecem perto do fim
         * do ataque e perto do início da liberação (gaze já perto do
         * fundo), aproximando "gaze ↓ → blink → hold → blink → retorno"
         * de VISUAL.md §3 sem descontinuidade visual. */
        const uint32_t attack_ms = 500u;
        const uint32_t release_ms = 900u;
        const float env = ease_envelope(elapsed, e->motif_duration_ms, attack_ms, release_ms);

        out->gaze_y += NB_IDLE_LOOK_DOWN_AMPLITUDE * env;
        if ((elapsed >= attack_ms - 50u && elapsed < attack_ms + 50u) ||
            (elapsed >= e->motif_duration_ms - release_ms - 100u &&
             elapsed < e->motif_duration_ms - release_ms)) {
            out->open_l = 0.0f;
            out->open_r = 0.0f;
        }
        break;
    }

    case NB_IDLE_MOTIF_SIDE_PEEK: {
        const float side =
            (e->motif_phase == 0u) ? NB_IDLE_SIDE_PEEK_AMPLITUDE : -NB_IDLE_SIDE_PEEK_AMPLITUDE;
        const float env = ease_envelope(elapsed, e->motif_duration_ms, 500u, 700u);

        out->gaze_x += side * env;
        break;
    }

    case NB_IDLE_MOTIF_VERTICAL_SCAN:
    case NB_IDLE_MOTIF_CROSS_SCAN: {
        /* Onda senoidal (não triangular): entra e sai em zero com
         * velocidade suave, sem reversão brusca no pico -- bug real
         * achado em bancada (a varredura linear "batia" nas pontas). */
        const float t = (float)elapsed / (float)e->motif_duration_ms;
        const float sweep = NB_IDLE_SCAN_AMPLITUDE * sinf(NB_IDLE_PI * t);

        out->gaze_y += sweep;
        if (e->active_motif == NB_IDLE_MOTIF_CROSS_SCAN) {
            out->gaze_x += sweep;
        }
        break;
    }

    case NB_IDLE_MOTIF_NONE:
    default:
        break;
    }

#if NB_IDLE_V2_SPIKE
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
     * (0) continua fechando de vez independente do nível. Em legado
     * energy_v2.level é sempre 0 -> fator 1.0, sem mudança de
     * comportamento. */
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
#endif
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
#if !NB_IDLE_V2_SPIKE
    engine->next_motif_at_ms = sample_next_motif_ms(engine);
#endif
    /* Sempre inicializada (mesmo com a flag desligada, ver
     * nb_idle_engine_t.attention_v2) -- semente derivada, xorshift32 não
     * pode receber o mesmo estado inicial de engine->rng_state sem
     * acoplar as duas sequências de sorteio. */
    nb_attention_init(&engine->attention_v2, engine->rng_state ^ 0x1234567u);
#if NB_IDLE_V2_SPIKE
    /* Blink×sacada (S3.7 completo, item 3) -- inócuo sob !NB_IDLE_V2_SPIKE
     * já que attention_v2 nunca é tickado ali. */
    nb_attention_set_saccade_callback(&engine->attention_v2, on_attention_saccade, engine);
#endif
    /* Semente derivada, distinta da de attention_v2 (mesma lógica acima). */
    nb_posture_init(&engine->posture_v2, engine->rng_state ^ 0x87654321u);
    nb_energy_init(&engine->energy_v2); /* sem RNG -- não precisa de semente */
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

#if NB_IDLE_V2_SPIKE
    /* Motor de atenção (RFC-VIDA-V2.md §7) substitui o SOFT_DRIFT: gaze de
     * fixação+sacada com viés de retorno ao centro, em vez do passeio
     * aleatório contínuo. Escreve direto em drift_x/y -- compute_output()
     * não precisa saber qual dos dois gerou a saída. */
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
#else
    /* SOFT_DRIFT: em vez de ruído tick-a-tick (que em bancada lia como
     * parado -- passo pequeno demais pra ser visível), o gaze desliza
     * suavemente rumo a um alvo dentro de ±NB_IDLE_DRIFT_AMPLITUDE,
     * resorteado a cada ~3-5s. Suavização exponencial: quanto maior
     * dt_ms/tau, mais rápido alcança o alvo. */
    if (engine->now_ms >= engine->drift_next_target_at_ms) {
        engine->drift_target_x =
            (rand01(&engine->rng_state) * 2.0f - 1.0f) * NB_IDLE_DRIFT_AMPLITUDE;
        engine->drift_target_y =
            (rand01(&engine->rng_state) * 2.0f - 1.0f) * NB_IDLE_DRIFT_AMPLITUDE;
        engine->drift_next_target_at_ms =
            engine->now_ms + 3000u + (uint32_t)(rand01(&engine->rng_state) * 2000.0f);
    }
    const float drift_tau_ms = 1500.0f; /* tempo pra cobrir ~63% do caminho até o alvo */
    const float drift_alpha = clampf((float)dt_ms / drift_tau_ms, 0.0f, 1.0f);
    engine->drift_x += (engine->drift_target_x - engine->drift_x) * drift_alpha;
    engine->drift_y += (engine->drift_target_y - engine->drift_y) * drift_alpha;
#endif

    if (engine->active_motif != NB_IDLE_MOTIF_NONE &&
        engine->now_ms - engine->motif_started_ms >= engine->motif_duration_ms) {
        engine->active_motif = NB_IDLE_MOTIF_NONE;
    }

    /* Blink independente e motif longo só disparam com o slot livre --
     * nenhum dos dois interrompe um motif em andamento. Sob o spike
     * (NB_IDLE_V2_SPIKE), o sorteio de motifs longos fica desligado (RFC
     * §7 -- motor de atenção substitui SIDE_PEEK/scans, respiração
     * substitui os demais); o blink independente continua. */
    if (engine->active_motif == NB_IDLE_MOTIF_NONE && engine->now_ms >= engine->next_blink_at_ms) {
        start_blink_motif(engine);
#if !NB_IDLE_V2_SPIKE
    } else if (engine->active_motif == NB_IDLE_MOTIF_NONE &&
              engine->now_ms >= engine->next_motif_at_ms) {
        start_long_motif(engine);
#endif
    }

    compute_output(engine, out);
}

const nb_idle_metrics_t *nb_idle_engine_get_metrics(const nb_idle_engine_t *engine)
{
    return (engine != NULL) ? &engine->metrics : NULL;
}
