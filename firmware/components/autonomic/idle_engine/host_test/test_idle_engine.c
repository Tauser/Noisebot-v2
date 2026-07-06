#include "../idle_engine.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(expr)                                                          \
    do {                                                                     \
        if (!(expr)) {                                                       \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);           \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

#define TICK_MS 20u

static void test_init_is_deterministic(void)
{
    nb_idle_engine_t a;
    nb_idle_engine_t b;

    nb_idle_engine_init(&a, 42);
    nb_idle_engine_init(&b, 42);
    /* Não dá pra memcmp a struct inteira aqui (S3.7 completo item 3): o
     * callback de blink×sacada guarda ctx=&engine, um ponteiro
     * self-referencial que necessariamente difere entre duas instâncias
     * em endereços diferentes -- byte igual não é o contrato, saída igual
     * é (comparada tick a tick abaixo). */

    for (int i = 0; i < 1000; ++i) {
        nb_idle_output_t out_a;
        nb_idle_output_t out_b;

        nb_idle_engine_tick(&a, TICK_MS, &out_a);
        nb_idle_engine_tick(&b, TICK_MS, &out_b);
        CHECK(memcmp(&out_a, &out_b, sizeof(out_a)) == 0);
    }
}

static void test_seed_zero_does_not_lock_rng(void)
{
    nb_idle_engine_t e;
    nb_idle_engine_t reference;

    nb_idle_engine_init(&e, 0);
    nb_idle_engine_init(&reference, 0x9E3779B9u);
    CHECK(e.rng_state == reference.rng_state);
    CHECK(e.rng_state != 0u);
}

static void test_null_is_safe(void)
{
    nb_idle_engine_init(NULL, 1);
    nb_idle_engine_set_mode(NULL, true, NB_IDLE_ATTENTION_ATTENTIVE);
    nb_idle_engine_tick(NULL, TICK_MS, NULL);
    CHECK(nb_idle_engine_get_metrics(NULL) == NULL);

    nb_idle_engine_t e;
    nb_idle_engine_init(&e, 7);
    nb_idle_engine_tick(&e, TICK_MS, NULL); /* out NULL com engine válido */
}

static void test_drift_never_exceeds_amplitude(void)
{
    nb_idle_engine_t e;

    /* Amplitude retunada em bancada (idle_engine.c,
     * NB_IDLE_DRIFT_AMPLITUDE) -- 0.04 literal de VISUAL.md §3 lia como
     * imperceptível na escala de pixels do renderer. */
    const float amplitude = 0.35f;

    nb_idle_engine_init(&e, 123);
    for (uint32_t ms = 0; ms < 120000u; ms += TICK_MS) {
        nb_idle_engine_tick(&e, TICK_MS, NULL);
        CHECK(e.drift_x >= -amplitude - 0.0001f && e.drift_x <= amplitude + 0.0001f);
        CHECK(e.drift_y >= -amplitude - 0.0001f && e.drift_y <= amplitude + 0.0001f);
    }
}

static void test_quiet_mode_roughly_halves_event_frequency(void)
{
    nb_idle_engine_t normal;
    nb_idle_engine_t quiet;
    const uint32_t duration_ms = 300000u; /* 5 min simulados */

    nb_idle_engine_init(&normal, 99);
    nb_idle_engine_init(&quiet, 99);
    nb_idle_engine_set_mode(&quiet, true, NB_IDLE_ATTENTION_IDLE);

    uint32_t normal_events = 0;
    uint32_t quiet_events = 0;

    for (uint32_t ms = 0; ms < duration_ms; ms += TICK_MS) {
        nb_idle_engine_tick(&normal, TICK_MS, NULL);
        nb_idle_engine_tick(&quiet, TICK_MS, NULL);
    }
    normal_events = normal.metrics.blink_bar_count + normal.metrics.double_blink_count +
                   normal.metrics.sustained_count + normal.metrics.look_down_count +
                   normal.metrics.line_blink_count + normal.metrics.side_peek_count +
                   normal.metrics.scan_count;
    quiet_events = quiet.metrics.blink_bar_count + quiet.metrics.double_blink_count +
                  quiet.metrics.sustained_count + quiet.metrics.look_down_count +
                  quiet.metrics.line_blink_count + quiet.metrics.side_peek_count +
                  quiet.metrics.scan_count;

    CHECK(normal_events > 0);
    CHECK(quiet_events > 0);
    /* "frequências ÷2" -- quiet deve ter nitidamente menos eventos, sem
     * exigir uma proporção exata (é estatística, não determinística). */
    CHECK(quiet_events < normal_events);
}

/* Os três testes a seguir cobrem o catálogo de motifs longos de S2.4
 * (VISUAL.md §3) -- só existem sob NB_IDLE_V2_SPIKE=0, já que o spike
 * (docs/ROADMAP.md "Plano S3.7 -- passo 0") desliga o sorteio desses
 * motifs. Rodar este arquivo com NB_IDLE_V2_SPIKE=0 (via
 * NB_HOST_TEST_CFLAGS) é o jeito de manter essa cobertura viva sem
 * duplicar o arquivo. */
#if !NB_IDLE_V2_SPIKE
static void test_attentive_schedules_long_motifs_more_often_than_idle(void)
{
    nb_idle_engine_t idle_engine;
    nb_idle_engine_t attentive_engine;
    const uint32_t duration_ms = 120000u; /* 2 min simulados */

    nb_idle_engine_init(&idle_engine, 55);
    nb_idle_engine_init(&attentive_engine, 55);
    nb_idle_engine_set_mode(&attentive_engine, false, NB_IDLE_ATTENTION_ATTENTIVE);

    for (uint32_t ms = 0; ms < duration_ms; ms += TICK_MS) {
        nb_idle_engine_tick(&idle_engine, TICK_MS, NULL);
        nb_idle_engine_tick(&attentive_engine, TICK_MS, NULL);
    }

    const nb_idle_metrics_t *idle_m = nb_idle_engine_get_metrics(&idle_engine);
    const nb_idle_metrics_t *att_m = nb_idle_engine_get_metrics(&attentive_engine);
    const uint32_t idle_long = idle_m->sustained_count + idle_m->look_down_count +
                               idle_m->line_blink_count + idle_m->side_peek_count +
                               idle_m->scan_count;
    const uint32_t att_long = att_m->sustained_count + att_m->look_down_count +
                              att_m->line_blink_count + att_m->side_peek_count +
                              att_m->scan_count;

    CHECK(att_long > idle_long);
}

/* Simula uma sessão em IDLE e verifica o critério de aceite de
 * VISUAL.md §3 (gate S2.4), sobre uma janela mais longa que os 60s do
 * ensaio de bancada e múltiplas sementes -- pra não depender de sorte de
 * uma semente só. A confirmação visual real continua sendo o ensaio de
 * bancada (fora do escopo deste host-test). */
static void test_acceptance_criteria_over_multiple_seeds(void)
{
    const uint32_t seeds[] = {1, 2, 3, 4, 5, 17, 99, 4242};
    /* 10 min por semente: com o intervalo médio de motif longo em IDLE
     * (15-40s), isso dá ~20 motifs longos por semente -- estatisticamente
     * robusto o bastante pra não depender de sorte de uma janela de 60s
     * isolada (a confirmação real da janela de 60s é o ensaio de bancada,
     * fora do escopo deste host-test). */
    const uint32_t duration_ms = 600000u;

    for (size_t s = 0; s < sizeof(seeds) / sizeof(seeds[0]); ++s) {
        nb_idle_engine_t e;

        nb_idle_engine_init(&e, seeds[s]);
        for (uint32_t ms = TICK_MS; ms <= duration_ms; ms += TICK_MS) {
            nb_idle_engine_tick(&e, TICK_MS, NULL);
        }

        const nb_idle_metrics_t *m = nb_idle_engine_get_metrics(&e);

        CHECK(m->sustained_count >= 1);
        CHECK(m->look_down_count + m->double_blink_count >= 1);
        CHECK(m->blink_bar_count >= 2);
    }
}
#endif /* !NB_IDLE_V2_SPIKE */

/* "nenhum intervalo de 15s com os olhos idênticos" (VISUAL.md §3) é
 * garantido por construção pelo SOFT_DRIFT contínuo, não por sorte no
 * agendamento de blink/motif: o passeio aleatório do drift muda a cada
 * tick, então os olhos nunca ficam bit-a-bit parados por muito tempo. */
static void test_soft_drift_keeps_changing_every_tick(void)
{
    nb_idle_engine_t e;
    nb_idle_engine_init(&e, 7);

    float prev_x = e.drift_x;
    float prev_y = e.drift_y;
    int changed_count = 0;

    for (int i = 0; i < 100; ++i) {
        nb_idle_engine_tick(&e, TICK_MS, NULL);
        if (e.drift_x != prev_x || e.drift_y != prev_y) {
            ++changed_count;
        }
        prev_x = e.drift_x;
        prev_y = e.drift_y;
    }
    /* Praticamente todo tick muda o drift (passeio aleatório contínuo);
     * uma tolerância pequena cobre o caso raro de amostra ~0 do RNG. */
    CHECK(changed_count >= 90);
}

#if !NB_IDLE_V2_SPIKE
static void test_curious_tilt_widens_exactly_one_eye(void)
{
    nb_idle_engine_t e;

    nb_idle_engine_init(&e, 321);
    /* Roda até pegar um CURIOUS_TILT (bounded: motifs longos disparam
     * dentro de 40s em IDLE; 5 min de folga é generoso). */
    bool saw_curious_tilt = false;
    for (uint32_t ms = 0; ms < 300000u && !saw_curious_tilt; ms += TICK_MS) {
        nb_idle_output_t out;
        nb_idle_engine_tick(&e, TICK_MS, &out);
        if (out.active_motif == NB_IDLE_MOTIF_CURIOUS_TILT) {
            saw_curious_tilt = true;
            const int widened_l = out.width_l > 1.0f;
            const int widened_r = out.width_r > 1.0f;
            CHECK(widened_l != widened_r); /* exatamente um olho */
        }
    }
    CHECK(saw_curious_tilt);
}
#endif /* !NB_IDLE_V2_SPIKE */

/* Testes do spike (S3.7 passo 0, docs/RFC-VIDA-V2.md §7): só fazem sentido
 * com NB_IDLE_V2_SPIKE ligado (config padrão do header). */
#if NB_IDLE_V2_SPIKE
/* Sorteio de motifs longos desligado -- só o blink independente (Poisson)
 * continua ativo, como exige o "Plano S3.7 -- passo 0" (item 1). */
static void test_spike_disables_long_motif_scheduling(void)
{
    nb_idle_engine_t e;

    nb_idle_engine_init(&e, 4242);
    for (uint32_t ms = 0; ms < 600000u; ms += TICK_MS) { /* 10 min simulados */
        nb_idle_engine_tick(&e, TICK_MS, NULL);
    }

    const nb_idle_metrics_t *m = nb_idle_engine_get_metrics(&e);

    CHECK(m->sustained_count == 0u);
    CHECK(m->look_down_count == 0u);
    CHECK(m->line_blink_count == 0u);
    CHECK(m->side_peek_count == 0u);
    CHECK(m->scan_count == 0u);
    CHECK(m->blink_bar_count + m->double_blink_count > 0u); /* blink continua vivo */
}

/* Respiração (RFC §7): open_l/open_r oscilam continuamente dentro de
 * 1 +- 2% quando os olhos não estão em blink (motif == NONE). */
static void test_spike_breath_modulates_eye_openness(void)
{
    nb_idle_engine_t e;
    float min_open = 2.0f;
    float max_open = 0.0f;

    nb_idle_engine_init(&e, 17);
    for (uint32_t ms = 0; ms < 60000u; ms += TICK_MS) {
        nb_idle_output_t out;

        nb_idle_engine_tick(&e, TICK_MS, &out);
        if (out.active_motif == NB_IDLE_MOTIF_NONE) {
            CHECK(out.open_l >= 0.97f && out.open_l <= 1.03f);
            CHECK(out.open_r >= 0.97f && out.open_r <= 1.03f);
            if (out.open_l < min_open) {
                min_open = out.open_l;
            }
            if (out.open_l > max_open) {
                max_open = out.open_l;
            }
        }
    }
    /* Se a respiração está de fato oscilando (não travada em 1.0), o
     * mínimo e o máximo observados precisam se afastar visivelmente. */
    CHECK(max_open - min_open > 0.02f);
}

/* Callback de sacada (RFC §7, "acoplamentos: blink×sacada") exposto até a
 * casca -- aqui só confirmamos que o motor de atenção embutido no
 * idle_engine dispara o gancho durante uma sessão normal de tick(). */
static void test_spike_attention_gaze_moves_over_time(void)
{
    nb_idle_engine_t e;
    int changed_count = 0;
    float prev_x;
    float prev_y;

    nb_idle_engine_init(&e, 8);
    nb_idle_engine_tick(&e, TICK_MS, NULL);
    prev_x = e.drift_x;
    prev_y = e.drift_y;

    for (int i = 0; i < 500; ++i) {
        nb_idle_engine_tick(&e, TICK_MS, NULL);
        if (e.drift_x != prev_x || e.drift_y != prev_y) {
            ++changed_count;
        }
        prev_x = e.drift_x;
        prev_y = e.drift_y;
    }
    CHECK(changed_count > 0);
}

/* Postura (RFC §7, motor 2, "Plano S3.7 completo" item 1) contribui pro
 * gaze/tilt/width -- confere que a saída se move ao longo do tempo (a
 * cobertura fina de envelope/não-repetição já mora em test_nb_posture.c;
 * aqui só a integração com o idle_engine). */
static void test_spike_posture_contributes_to_output(void)
{
    nb_idle_engine_t e;
    nb_idle_output_t first;
    int tilt_changed = 0;
    int width_asymmetric_at_some_point = 0;

    nb_idle_engine_init(&e, 999);
    nb_idle_engine_tick(&e, TICK_MS, &first);

    for (uint32_t ms = 0; ms < 300000u; ms += TICK_MS) { /* 5 min simulados */
        nb_idle_output_t out;

        nb_idle_engine_tick(&e, TICK_MS, &out);
        if (out.tilt != first.tilt) {
            tilt_changed = 1;
        }
        if (out.width_l != out.width_r) {
            width_asymmetric_at_some_point = 1;
        }
    }
    CHECK(tilt_changed);
    CHECK(width_asymmetric_at_some_point);
}

/* Invariante H7: nb_idle_engine_reset_transient() zera a contribuição da
 * postura de volta ao centro (RFC §7: "entrada em IDLE reseta ao
 * centro"). */
static void test_spike_reset_transient_zeroes_posture(void)
{
    nb_idle_engine_t e;

    nb_idle_engine_init(&e, 321);
    for (uint32_t ms = 0; ms < 200000u; ms += TICK_MS) {
        nb_idle_engine_tick(&e, TICK_MS, NULL);
    }
    /* Confere que a postura realmente saiu do centro antes do reset. */
    CHECK(e.posture_v2.roll != 0.0f || e.posture_v2.gaze_x != 0.0f ||
         e.posture_v2.gaze_y != 0.0f || e.posture_v2.asymmetry != 0.0f);

    nb_idle_engine_reset_transient(&e);
    CHECK(e.posture_v2.roll == 0.0f);
    CHECK(e.posture_v2.gaze_x == 0.0f && e.posture_v2.gaze_y == 0.0f);
    CHECK(e.posture_v2.asymmetry == 0.0f);

    /* out.tilt não é mais só a postura (item 3 "acoplamentos" somou
     * roll-segue-gaze, que reset_transient também zera -- ver abaixo --
     * mas volta a se afastar de 0 no tick seguinte, seguindo o gaze da
     * atenção, que não é resetada aqui de propósito: só a postura é
     * invariante H7 nesta etapa). width_l/width_r continuam simétricos
     * porque só a postura contribui pra assimetria. */
    CHECK(e.roll_gaze_lag_x == 0.0f);
    nb_idle_output_t out;
    nb_idle_engine_tick(&e, TICK_MS, &out);
    CHECK(out.width_l == out.width_r);
}

/* Energia (RFC §7, motor 3, "Plano S3.7 completo" item 2): tédio
 * crescente sem estímulo (via nb_idle_engine_set_energy_inputs)
 * desacelera o blink e faz a pálpebra descansar mais fechada, comparado
 * a um motor "alerta" (sem entradas de energia, defaults 0/0.0). */
static void test_spike_energy_droops_eyelid_and_slows_blink(void)
{
    nb_idle_engine_t alert;
    nb_idle_engine_t bored;
    const uint32_t duration_ms = 900000u; /* 15 min simulados */

    nb_idle_engine_init(&alert, 111);
    nb_idle_engine_init(&bored, 111);

    for (uint32_t ms = 0; ms < duration_ms; ms += TICK_MS) {
        nb_idle_engine_set_energy_inputs(&bored, ms, 0.0f); /* tédio cresce com o tempo */
        nb_idle_engine_tick(&alert, TICK_MS, NULL);
        nb_idle_engine_tick(&bored, TICK_MS, NULL);
    }

    CHECK(bored.energy_v2.level > 0.5f); /* de fato ficou sonolento */
    CHECK(alert.energy_v2.level == 0.0f); /* nunca alimentado -- nunca sonolento */

    const nb_idle_metrics_t *m_alert = nb_idle_engine_get_metrics(&alert);
    const nb_idle_metrics_t *m_bored = nb_idle_engine_get_metrics(&bored);
    const uint32_t blinks_alert = m_alert->blink_bar_count + m_alert->double_blink_count;
    const uint32_t blinks_bored = m_bored->blink_bar_count + m_bored->double_blink_count;

    CHECK(blinks_bored < blinks_alert); /* blink mais espaçado com sono */

    /* Pálpebra de descanso (motif == NONE) mais fechada no sonolento. */
    nb_idle_output_t out_alert;
    nb_idle_output_t out_bored;
    int compared = 0;
    for (uint32_t ms = 0; ms < 20000u && !compared; ms += TICK_MS) {
        nb_idle_engine_set_energy_inputs(&bored, duration_ms, 0.0f);
        nb_idle_engine_tick(&alert, TICK_MS, &out_alert);
        nb_idle_engine_tick(&bored, TICK_MS, &out_bored);
        if (out_alert.active_motif == NB_IDLE_MOTIF_NONE &&
            out_bored.active_motif == NB_IDLE_MOTIF_NONE) {
            CHECK(out_bored.open_l < out_alert.open_l);
            compared = 1;
        }
    }
    CHECK(compared);
}

/* Blink×sacada (RFC §7, "acoplamentos", item 3): a maioria das
 * transições FIXATE->SACCADE do motor de atenção dispara um blink de
 * fato (slot livre) -- não é 100% porque, raramente, um blink
 * independente já está em curso quando a sacada começa. */
static void test_spike_saccade_triggers_blink(void)
{
    nb_idle_engine_t e;
    uint32_t saccades = 0;
    uint32_t saccades_with_blink = 0;

    nb_idle_engine_init(&e, 17);
    for (uint32_t ms = 0; ms < 600000u; ms += TICK_MS) { /* 10 min simulados */
        const nb_attention_phase_t before = e.attention_v2.phase;
        const nb_idle_motif_t motif_before = e.active_motif;

        nb_idle_engine_tick(&e, TICK_MS, NULL);

        if (before == NB_ATTENTION_FIXATE && e.attention_v2.phase == NB_ATTENTION_SACCADE) {
            ++saccades;
            if (motif_before == NB_IDLE_MOTIF_NONE &&
                (e.active_motif == NB_IDLE_MOTIF_BLINK_BAR ||
                 e.active_motif == NB_IDLE_MOTIF_DOUBLE_BLINK)) {
                ++saccades_with_blink;
            }
        }
    }
    CHECK(saccades > 20); /* estatisticamente robusto sobre 10 min simulados */
    /* Tolerância: nem toda sacada pega o slot livre (blink independente
     * pode já estar em curso), mas a esmagadora maioria deve pegar. */
    CHECK(saccades_with_blink > saccades * 8u / 10u);
}

/* Roll segue gaze com atraso (RFC §7, "acoplamentos", item 3): o filtro
 * passa-baixa nunca sai do envelope do gaze e nunca copia o valor
 * instantâneo (prova que é atraso, não passagem direta). */
static void test_spike_roll_follows_gaze_with_lag(void)
{
    nb_idle_engine_t e;
    int lag_differs_from_instantaneous_gaze = 0;

    nb_idle_engine_init(&e, 4);
    for (uint32_t ms = 0; ms < 60000u; ms += TICK_MS) {
        nb_idle_engine_tick(&e, TICK_MS, NULL);
        CHECK(e.roll_gaze_lag_x >= -NB_ATTENTION_ENVELOPE - 0.01f &&
             e.roll_gaze_lag_x <= NB_ATTENTION_ENVELOPE + 0.01f);
        if (e.roll_gaze_lag_x != e.drift_x) {
            lag_differs_from_instantaneous_gaze = 1;
        }
    }
    CHECK(lag_differs_from_instantaneous_gaze);
}

/* Gestos nomeados (RFC §7, "Plano S3.7 completo" item 4): cada um
 * dispara dentro do próprio intervalo, nunca dois ao mesmo tempo (mesmo
 * slot exclusivo de active_motif), e quiet_mode dobra os intervalos --
 * mesma verificação estatística já usada pro blink/motifs antigos. */
static void test_spike_named_gestures_fire(void)
{
    nb_idle_engine_t e;
    const uint32_t duration_ms = 3600000u; /* 60 min simulados */

    nb_idle_engine_init(&e, 2024);
    for (uint32_t ms = 0; ms < duration_ms; ms += TICK_MS) {
        nb_idle_engine_tick(&e, TICK_MS, NULL);
    }

    /* "Nunca dois ao mesmo tempo" já é garantido estruturalmente por
     * active_motif ser um campo único (não um bitset) -- não há o que
     * testar em runtime além disso. O que vale conferir é que os três
     * gestos de fato disparam dentro da janela. */
    const nb_idle_metrics_t *m = nb_idle_engine_get_metrics(&e);
    /* Em 60 min, CHECK_IN (1-3min) deve disparar bastante mais vezes que
     * SLOW_BLINK/SIGH (30-90s/45-120s teriam mais chances, mas todos
     * disputam o mesmo slot com blink -- só confere presença, não conta
     * exata). */
    CHECK(m->check_in_count >= 1u);
    CHECK(m->slow_blink_count >= 1u);
    CHECK(m->sigh_count >= 1u);
}

/* quiet_mode dobra os intervalos dos três gestos -- menos disparos na
 * mesma janela, mesmo critério estatístico de
 * test_quiet_mode_roughly_halves_event_frequency(). */
static void test_spike_quiet_mode_slows_named_gestures(void)
{
    nb_idle_engine_t normal;
    nb_idle_engine_t quiet;
    const uint32_t duration_ms = 3600000u; /* 60 min simulados */

    nb_idle_engine_init(&normal, 77);
    nb_idle_engine_init(&quiet, 77);
    nb_idle_engine_set_mode(&quiet, true, NB_IDLE_ATTENTION_IDLE);

    for (uint32_t ms = 0; ms < duration_ms; ms += TICK_MS) {
        nb_idle_engine_tick(&normal, TICK_MS, NULL);
        nb_idle_engine_tick(&quiet, TICK_MS, NULL);
    }

    const nb_idle_metrics_t *m_normal = nb_idle_engine_get_metrics(&normal);
    const nb_idle_metrics_t *m_quiet = nb_idle_engine_get_metrics(&quiet);
    const uint32_t gestures_normal =
        m_normal->check_in_count + m_normal->slow_blink_count + m_normal->sigh_count;
    const uint32_t gestures_quiet =
        m_quiet->check_in_count + m_quiet->slow_blink_count + m_quiet->sigh_count;

    CHECK(gestures_normal > 0u);
    CHECK(gestures_quiet < gestures_normal);
}
#endif /* NB_IDLE_V2_SPIKE */

int main(void)
{
    test_init_is_deterministic();
    test_seed_zero_does_not_lock_rng();
    test_null_is_safe();
    test_drift_never_exceeds_amplitude();
    test_quiet_mode_roughly_halves_event_frequency();
    test_soft_drift_keeps_changing_every_tick();

#if !NB_IDLE_V2_SPIKE
    test_attentive_schedules_long_motifs_more_often_than_idle();
    test_acceptance_criteria_over_multiple_seeds();
    test_curious_tilt_widens_exactly_one_eye();
#endif

#if NB_IDLE_V2_SPIKE
    test_spike_disables_long_motif_scheduling();
    test_spike_breath_modulates_eye_openness();
    test_spike_attention_gaze_moves_over_time();
    test_spike_posture_contributes_to_output();
    test_spike_reset_transient_zeroes_posture();
    test_spike_energy_droops_eyelid_and_slows_blink();
    test_spike_saccade_triggers_blink();
    test_spike_roll_follows_gaze_with_lag();
    test_spike_named_gestures_fire();
    test_spike_quiet_mode_slows_named_gestures();
#endif

    if (failures == 0) {
        printf("idle_engine host_test: ok\n");
        return 0;
    }

    printf("idle_engine host_test: %d failure(s)\n", failures);
    return 1;
}
