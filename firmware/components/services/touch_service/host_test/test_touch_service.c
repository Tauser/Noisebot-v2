#include "../touch_service.h"

#include <stdio.h>

static int failures;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);           \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

#define TICK_MS 20u
#define BASELINE 1000u

/* Passa o boot stabilization (1000ms) com o sensor "solto" (raw=baseline). */
static void settle_boot(nb_touch_service_t *svc)
{
    for (uint32_t t = 0; t < 1100u; t += TICK_MS) {
        nb_touch_event_t evt;
        nb_touch_service_tick(svc, BASELINE, TICK_MS, &evt);
    }
}

/* Passa a janela de recalib_boot (2000ms) e o ruído mínimo pra estabilizar
 * a janela (NB_TOUCH_SERVICE_NOISE_WINDOW=12 amostras). */
static void settle_recalib_window(nb_touch_service_t *svc, uint32_t raw)
{
    for (uint32_t t = 0; t < 2100u; t += TICK_MS) {
        nb_touch_event_t evt;
        nb_touch_service_tick(svc, raw, TICK_MS, &evt);
    }
}

/* Roda N ticks com raw fixo, retorna true se algum evento disparou (com o
 * primeiro evento em *out_evt). */
static bool run_ticks(nb_touch_service_t *svc, uint32_t raw, uint32_t total_ms,
                      nb_touch_event_t *out_evt)
{
    for (uint32_t t = 0; t < total_ms; t += TICK_MS) {
        nb_touch_event_t evt;
        if (nb_touch_service_tick(svc, raw, TICK_MS, &evt)) {
            if (out_evt != NULL) {
                *out_evt = evt;
            }
            return true;
        }
    }
    return false;
}

static uint32_t touched_raw(const nb_touch_service_t *svc)
{
    /* Acima do threshold_on com folga confortável. */
    return svc->threshold_on + (svc->threshold_on - svc->baseline) + 50u;
}

static void test_init_is_idle_with_baseline(void)
{
    nb_touch_service_t svc;

    nb_touch_service_init(&svc, BASELINE);
    CHECK(svc.baseline == BASELINE);
    CHECK(!nb_touch_service_is_pressed(&svc));
    CHECK(nb_touch_service_get_duration_ms(&svc) == 0);
    CHECK(svc.threshold_on > svc.baseline);
    CHECK(svc.threshold_off > svc.baseline);
    CHECK(svc.threshold_off < svc.threshold_on);
}

static void test_boot_stabilization_suppresses_events(void)
{
    nb_touch_service_t svc;
    nb_touch_service_init(&svc, BASELINE);

    /* Antes de estabilizar, mesmo com raw bem acima do threshold, nenhum
     * evento pode disparar. */
    nb_touch_event_t evt;
    bool fired = false;
    for (uint32_t t = 0; t < 999u; t += TICK_MS) {
        if (nb_touch_service_tick(&svc, touched_raw(&svc), TICK_MS, &evt)) {
            fired = true;
        }
    }
    CHECK(!fired);
}

static void test_tap_fires_after_debounce_and_hold(void)
{
    nb_touch_service_t svc;
    nb_touch_service_init(&svc, BASELINE);
    settle_boot(&svc);

    nb_touch_event_t evt;
    CHECK(run_ticks(&svc, touched_raw(&svc), 500u, &evt));
    CHECK(evt == NB_TOUCH_EVENT_TAP);
    CHECK(nb_touch_service_is_pressed(&svc));
}

/* Um pico isolado de 1 amostra (ex.: ruído de RF, ver bugs reais do S2.1)
 * nunca confirma debounce_on (exige 3 amostras consecutivas) -- não deve
 * disparar nada nem sair de IDLE. A rejeição de proximidade real (sinal
 * que confirma o toque mas cai antes do hold mínimo) é um comportamento
 * emergente que depende da forma real do sinal capacitivo, não de um
 * degrau sintético -- fica coberta pelo gate de bancada (zero falso
 * positivo em 1h de ruído ambiente), não por este host-test. */
static void test_isolated_noise_spike_never_confirms_touch(void)
{
    nb_touch_service_t svc;
    nb_touch_service_init(&svc, BASELINE);
    settle_boot(&svc);

    const uint32_t spike = touched_raw(&svc);
    nb_touch_event_t evt;
    bool fired = false;

    for (int cycle = 0; cycle < 10; ++cycle) {
        if (nb_touch_service_tick(&svc, spike, TICK_MS, &evt)) {
            fired = true;
        }
        for (int i = 0; i < 5; ++i) { /* volta pro baseline antes do próximo pico */
            if (nb_touch_service_tick(&svc, BASELINE, TICK_MS, &evt)) {
                fired = true;
            }
        }
    }
    CHECK(!fired);
    CHECK(svc.state == NB_TOUCH_STATE_IDLE);
}

static void test_long_press_then_sustained_sequence(void)
{
    nb_touch_service_t svc;
    nb_touch_service_init(&svc, BASELINE);
    settle_boot(&svc);

    const uint32_t raw = touched_raw(&svc);
    nb_touch_event_t evt;

    CHECK(run_ticks(&svc, raw, 500u, &evt));
    CHECK(evt == NB_TOUCH_EVENT_TAP);

    CHECK(run_ticks(&svc, raw, 2000u, &evt));
    CHECK(evt == NB_TOUCH_EVENT_LONG_PRESS);

    CHECK(run_ticks(&svc, raw, 2000u, &evt));
    CHECK(evt == NB_TOUCH_EVENT_SUSTAINED);

    /* Solta: volta a IDLE. */
    nb_touch_event_t release_evt;
    bool fired_on_release = run_ticks(&svc, BASELINE, 200u, &release_evt);
    CHECK(!fired_on_release);
    CHECK(!nb_touch_service_is_pressed(&svc));
}

static void test_sleeping_fires_wake_instead_of_tap(void)
{
    nb_touch_service_t svc;
    nb_touch_service_init(&svc, BASELINE);
    settle_boot(&svc);
    nb_touch_service_set_sleeping(&svc, true);

    nb_touch_event_t evt;
    CHECK(run_ticks(&svc, touched_raw(&svc), 500u, &evt));
    CHECK(evt == NB_TOUCH_EVENT_WAKE);
}

static void test_hysteresis_rejects_chatter_near_threshold(void)
{
    nb_touch_service_t svc;
    nb_touch_service_init(&svc, BASELINE);
    settle_boot(&svc);

    /* Oscila logo abaixo de threshold_on -- nunca confirma debounce_on. */
    const uint32_t near_on = svc.threshold_on - 1u;
    nb_touch_event_t evt;
    bool fired = false;
    for (int i = 0; i < 50; ++i) {
        const uint32_t raw = (i % 2 == 0) ? near_on : BASELINE;
        if (nb_touch_service_tick(&svc, raw, TICK_MS, &evt)) {
            fired = true;
        }
    }
    CHECK(!fired);
}

static void test_slow_recalibration_moves_baseline_when_idle_stable(void)
{
    /* Baseline e drift na escala real de hardware (contagem bruta do
     * touch v2 do ESP32-S3 fica na casa de dezenas de milhares) -- com
     * BASELINE=1000 pequeno, RECALIB_ALPHA=0.001 arredonda o incremento
     * pra zero a cada tick (bug de quantização do teste, não do núcleo:
     * o mesmo formato de cálculo do v1 só se move quando
     * alpha*(sinal-baseline) >= 0.5, ou seja, precisa de um delta grande
     * o bastante em termos absolutos). */
    const uint32_t hw_baseline = 50000u;
    const uint32_t drifted = hw_baseline + 2000u; /* 4%, dentro do teto de threshold_off */

    nb_touch_service_t svc;
    nb_touch_service_init(&svc, hw_baseline);
    settle_boot(&svc);
    settle_recalib_window(&svc, drifted);

    const uint32_t baseline_after = svc.baseline;
    /* Roda bastante mais tempo estável -- baseline deve se mover em
     * direção a "drifted", não ficar parado no valor original. */
    for (uint32_t t = 0; t < 30000u; t += TICK_MS) {
        nb_touch_event_t evt;
        nb_touch_service_tick(&svc, drifted, TICK_MS, &evt);
    }
    CHECK(svc.baseline > baseline_after);
    CHECK(svc.baseline <= drifted);
}

static void test_no_recalibration_during_active_touch(void)
{
    nb_touch_service_t svc;
    nb_touch_service_init(&svc, BASELINE);
    settle_boot(&svc);
    settle_recalib_window(&svc, BASELINE);

    const uint32_t baseline_before = svc.baseline;
    /* Mantém tocado (acima do threshold) por um tempo longo -- proteção
     * contra poisoning: baseline não pode se mover em direção ao toque. */
    for (uint32_t t = 0; t < 5000u; t += TICK_MS) {
        nb_touch_event_t evt;
        nb_touch_service_tick(&svc, touched_raw(&svc), TICK_MS, &evt);
    }
    CHECK(svc.baseline == baseline_before);
}

static void test_null_is_safe(void)
{
    nb_touch_event_t evt;

    nb_touch_service_init(NULL, BASELINE);
    nb_touch_service_set_sensitivity(NULL, 0.5f);
    nb_touch_service_set_sleeping(NULL, true);
    CHECK(!nb_touch_service_tick(NULL, 1234, TICK_MS, &evt));
    CHECK(!nb_touch_service_is_pressed(NULL));
    CHECK(nb_touch_service_get_duration_ms(NULL) == 0);
}

int main(void)
{
    test_init_is_idle_with_baseline();
    test_boot_stabilization_suppresses_events();
    test_tap_fires_after_debounce_and_hold();
    test_isolated_noise_spike_never_confirms_touch();
    test_long_press_then_sustained_sequence();
    test_sleeping_fires_wake_instead_of_tap();
    test_hysteresis_rejects_chatter_near_threshold();
    test_slow_recalibration_moves_baseline_when_idle_stable();
    test_no_recalibration_during_active_touch();
    test_null_is_safe();

    if (failures == 0) {
        printf("touch_service host_test: ok\n");
        return 0;
    }

    printf("touch_service host_test: %d failure(s)\n", failures);
    return 1;
}
