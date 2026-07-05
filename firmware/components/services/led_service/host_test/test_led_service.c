#include "../led_service.h"

#include <math.h>
#include <stdio.h>

static int failures;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);           \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static uint8_t expected_gamma_byte(float base_0_255, float level, float brightness_scale) {
    float frac = (base_0_255 / 255.0f) * level * brightness_scale;
    if (frac < 0.0f) {
        frac = 0.0f;
    }
    if (frac > 1.0f) {
        frac = 1.0f;
    }
    float corrected = powf(frac, 2.2f);
    return (uint8_t)(corrected * 255.0f + 0.5f);
}

static int close_u8(uint8_t a, uint8_t b, int tol) {
    int diff = (int)a - (int)b;
    if (diff < 0) {
        diff = -diff;
    }
    return diff <= tol;
}

static void test_init_defaults(void) {
    nb_led_service_t svc;
    nb_led_frame_t frame;

    nb_led_service_init(&svc);
    CHECK(svc.state == NB_FSM_STATE_BOOT);
    CHECK(svc.brightness_scale > 0.999f && svc.brightness_scale < 1.001f);

    const bool dirty = nb_led_service_tick(&svc, 16u, &frame);
    CHECK(dirty); /* primeiro frame sempre é "novo" */
}

static void test_touch_reacting_static_full_brightness(void) {
    nb_led_service_t svc;
    nb_led_frame_t frame;

    nb_led_service_init(&svc);
    nb_led_service_set_state(&svc, NB_FSM_STATE_TOUCH_REACTING);
    nb_led_service_tick(&svc, 33u, &frame);

    /* min_level=1.0, sem onda -- cor base (255,140,60) integral, gamma
     * aplicado. */
    CHECK(close_u8(frame.pixels[0].r, expected_gamma_byte(255.0f, 1.0f, 1.0f), 1));
    CHECK(close_u8(frame.pixels[0].g, expected_gamma_byte(140.0f, 1.0f, 1.0f), 1));
    CHECK(close_u8(frame.pixels[0].b, expected_gamma_byte(60.0f, 1.0f, 1.0f), 1));
    CHECK(frame.pixels[0].r == frame.pixels[1].r); /* STATIC não tem defasagem entre LEDs */
}

static void test_error_blinks_between_full_and_floor(void) {
    nb_led_service_t svc;
    nb_led_frame_t frame;

    nb_led_service_init(&svc);
    nb_led_service_set_state(&svc, NB_FSM_STATE_ERROR);

    /* t=0 (< period/2 = 300ms) -- fase "ligada", nível 1.0. */
    nb_led_service_tick(&svc, 0u, &frame);
    CHECK(close_u8(frame.pixels[0].r, expected_gamma_byte(255.0f, 1.0f, 1.0f), 1));
    CHECK(frame.pixels[0].g == 0u); /* canal 0 na cor-base permanece 0 em qualquer nível */
    CHECK(frame.pixels[0].b == 0u);

    /* Avança pra fase "desligada" (>= 300ms) -- nível cai pro piso 0.05,
     * nunca preto total (evita leitura de falha de driver). */
    nb_led_service_tick(&svc, 350u, &frame);
    CHECK(close_u8(frame.pixels[0].r, expected_gamma_byte(255.0f, 0.12f, 1.0f), 1));
    CHECK(frame.pixels[0].r > 0u);
    CHECK(frame.pixels[0].r < expected_gamma_byte(255.0f, 1.0f, 1.0f));
}

static void test_state_change_resets_phase_same_state_does_not(void) {
    nb_led_service_t svc;
    nb_led_frame_t frame_a;
    nb_led_frame_t frame_b;

    nb_led_service_init(&svc);
    nb_led_service_set_state(&svc, NB_FSM_STATE_IDLE);
    nb_led_service_tick(&svc, 3000u, &frame_a); /* meio do período de 6000ms */

    /* Repetir o mesmo estado não reseta a fase -- o próximo tick continua
     * de onde estava, não pula de volta pro início da onda. */
    nb_led_service_set_state(&svc, NB_FSM_STATE_IDLE);
    nb_led_service_tick(&svc, 1u, &frame_b);
    CHECK(svc.state_elapsed_ms == 3001u);

    /* Trocar de estado de verdade reseta a fase pra 0. */
    nb_led_service_set_state(&svc, NB_FSM_STATE_ATTENTIVE);
    CHECK(svc.state_elapsed_ms == 0u);
}

static void test_touch_overlay_triggers_and_fades_back(void) {
    nb_led_service_t svc;
    nb_led_frame_t frame;

    nb_led_service_init(&svc);
    nb_led_service_set_state(&svc, NB_FSM_STATE_SLEEPING); /* base bem escuro, overlay some evidente */
    nb_led_service_tick(&svc, 10u, &frame);

    nb_led_service_trigger_touch(&svc);
    CHECK(svc.touch_overlay_active);

    /* Pico do flash (~150ms): overlay quase totalmente sobrepõe o base. */
    nb_led_service_tick(&svc, 150u, &frame);
    CHECK(close_u8(frame.pixels[0].r, expected_gamma_byte(255.0f, 1.0f, 1.0f), 2));

    /* Depois do total (1050ms), overlay desativa sozinho e volta ao base
     * escuro de SLEEPING. */
    nb_led_service_tick(&svc, 1000u, &frame);
    CHECK(!svc.touch_overlay_active);
    CHECK(frame.pixels[0].r < expected_gamma_byte(255.0f, 1.0f, 1.0f));
}

static void test_touch_overlay_ignored_during_error_and_safe_mode(void) {
    nb_led_service_t svc;
    nb_led_frame_t frame;

    nb_led_service_init(&svc);
    nb_led_service_set_state(&svc, NB_FSM_STATE_ERROR);
    nb_led_service_trigger_touch(&svc);
    CHECK(!svc.touch_overlay_active);

    nb_led_service_init(&svc);
    nb_led_service_set_state(&svc, NB_FSM_STATE_SAFE_MODE);
    nb_led_service_trigger_touch(&svc);
    CHECK(!svc.touch_overlay_active);

    (void)frame;
}

static void test_brightness_scale_clamps_and_applies(void) {
    nb_led_service_t svc;
    nb_led_frame_t frame;

    nb_led_service_init(&svc);
    nb_led_service_set_state(&svc, NB_FSM_STATE_TOUCH_REACTING);

    nb_led_service_set_brightness_scale(&svc, 2.0f); /* clampa pra 1.0 */
    CHECK(svc.brightness_scale > 0.999f && svc.brightness_scale < 1.001f);

    nb_led_service_set_brightness_scale(&svc, -1.0f); /* clampa pra 0.0 */
    CHECK(svc.brightness_scale < 0.001f);

    nb_led_service_tick(&svc, 16u, &frame);
    CHECK(frame.pixels[0].r == 0u);
    CHECK(frame.pixels[0].g == 0u);
    CHECK(frame.pixels[0].b == 0u);
}

static void test_dirty_flag_settles_on_static_state(void) {
    nb_led_service_t svc;
    nb_led_frame_t frame;

    nb_led_service_init(&svc);
    nb_led_service_set_state(&svc, NB_FSM_STATE_SLEEPING); /* STATIC -- sem oscilação */

    const bool first = nb_led_service_tick(&svc, 16u, &frame);
    CHECK(first); /* primeiro frame sempre dirty */

    const bool second = nb_led_service_tick(&svc, 16u, &frame);
    CHECK(!second); /* mesmo nível, mesmo frame -- casca não precisa reescrever o RMT */
}

static void test_null_is_safe(void) {
    nb_led_frame_t frame;

    nb_led_service_init(NULL);
    nb_led_service_set_state(NULL, NB_FSM_STATE_IDLE);
    nb_led_service_trigger_touch(NULL);
    nb_led_service_set_brightness_scale(NULL, 0.5f);
    CHECK(!nb_led_service_tick(NULL, 16u, &frame));

    nb_led_service_t svc;
    nb_led_service_init(&svc);
    CHECK(!nb_led_service_tick(&svc, 16u, NULL));
}

int main(void) {
    test_init_defaults();
    test_touch_reacting_static_full_brightness();
    test_error_blinks_between_full_and_floor();
    test_state_change_resets_phase_same_state_does_not();
    test_touch_overlay_triggers_and_fades_back();
    test_touch_overlay_ignored_during_error_and_safe_mode();
    test_brightness_scale_clamps_and_applies();
    test_dirty_flag_settles_on_static_state();
    test_null_is_safe();

    if (failures == 0) {
        printf("led_service host_test: ok\n");
        return 0;
    }

    printf("led_service host_test: %d failure(s)\n", failures);
    return 1;
}
