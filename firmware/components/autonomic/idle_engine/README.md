# idle_engine

Componente `firmware/components/autonomic/idle_engine` — S2.4, camada L4
(`ARCHITECTURE.md`). Núcleo C17 puro (`idle_engine.c/.h`): o catálogo de
motifs de `VISUAL.md` §3 — `SOFT_DRIFT` (contínuo), processo de Poisson
de blink (`BLINK_BAR`/`DOUBLE_BLINK`, média 5s, piso 1.8s, ~30% viram
double) e o agendamento ponderado dos motifs longos (`CURIOUS_TILT` 30%,
`HEAD_TILT_HOLD` 20%, `LOOK_DOWN_BLINK` 15%, `LINE_BLINK` 15%,
`SIDE_PEEK` 10%, `VERTICAL_SCAN`/`CROSS_SCAN` 5%+5%, a cada 15-40s em
`IDLE` ou 5-13s em `ATTENTIVE`; modo `quiet` dobra os intervalos).

**Amplitudes retunadas em bancada** (`NB_IDLE_DRIFT_AMPLITUDE` e
similares, no topo de `idle_engine.c`): os valores literais de
`VISUAL.md` §3 (drift ≤0.04, motifs 0.3-0.4) mapeavam pra menos de 1px
na escala de pixel do renderer e liam como parado/curto demais ao vivo;
mantém a intenção do documento (drift sutil, motifs claramente vivos),
não os números literais. O drift também deixou de ser ruído tick-a-tick
e virou um passeio suave rumo a alvos re-sorteados a cada 3-5s.

Sem FreeRTOS/ESP-IDF: clock injetado via `nb_idle_engine_tick(dt_ms)` e
RNG embutido (xorshift32 determinístico, semente injetada no init) —
host-testável byte a byte, sem depender de `esp_random()`. Sem casca
própria ainda (mesma regra do `event_bus`/`tiny_fsm`); `main.c` liga
direto ao `face_renderer` (S2.2) pra bring-up, somando todos os motifs
(drift, blink, gaze, largura por olho, roll) à expressão `NEUTRAL`.

Host-test cobre: determinismo (mesma semente ⇒ mesma sequência),
semente 0 não trava o RNG, `NULL` seguro, drift dentro da amplitude,
modo `quiet` reduz nitidamente a frequência de eventos, `ATTENTIVE`
agenda motifs longos mais seguido que `IDLE`, `CURIOUS_TILT` sempre
larga exatamente um olho, e o critério de aceite de `VISUAL.md` §3
(≥1 expressão sustentada, ≥1 `LOOK_DOWN_BLINK`/double blink, ≥2
`BLINK_BAR`) verificado sobre 10 min simulados × 8 sementes — robusto
o bastante pra não depender de sorte de uma janela de 60s isolada.

**Gate do S2.4 fechado:** todos os motifs confirmados em bancada pelo
usuário, incluindo `CURIOUS_TILT` (largura) e `HEAD_TILT_HOLD` (roll)
depois de o renderer (S2.2) ganhar suporte a `width_l`/`width_r`/`tilt`
em `nb_face_renderer_shell_draw()`.

## S3.7 — spike go/no-go (idle v2)

Flag de compilação `NB_IDLE_V2_SPIKE` (`idle_engine.h`, ligada por padrão
nesta fase): desliga o sorteio dos motifs longos de S2.4 e liga dois
núcleos novos, também C17 puros e host-testáveis isoladamente
(`docs/RFC-VIDA-V2.md` §7):

- **`nb_breath.c/.h`** — função pura `nb_breath_scale(now_ms, period_ms,
  amp)`: fator multiplicativo (1 ± 2%, período ~6s) aplicado sobre
  `open_l`/`open_r` depois do blink, modulando a altura dos olhos.
- **`nb_attention.c/.h`** — FSM `FIXATE⇄SACCADE`: fixação de 0.5-3s com
  micro-tremor (≤0.02), sacada de 80ms (ease-out) para um alvo em
  `[-0.35,0.35]²` com ~40% de viés de retorno ao centro. Alimenta
  `drift_x`/`drift_y` no lugar do `SOFT_DRIFT` antigo. Expõe
  `nb_attention_set_saccade_callback()` (gancho pra acoplar blink×sacada
  — só exposto e testado neste spike, o acoplamento real fica pro "Plano
  S3.7 completo" pós-go).

O blink independente (Poisson) **não muda** nas duas configs. `main.c`
continua chamando `nb_idle_engine_tick()` sem nenhuma alteração — a troca
de comportamento é inteira interna ao núcleo (`idle_engine.c` decide via
`#if NB_IDLE_V2_SPIKE` o que alimenta `drift_x`/`drift_y` e se aplica
`nb_breath_scale()`), preservando "zero mudança fora do idle_engine" do
escopo do spike.

Rodar a suíte de host-tests nas duas configs (`tools/run_host_tests.py`):

```
python tools/run_host_tests.py                                    # spike ligado (padrão do header)
NB_HOST_TEST_CFLAGS="-DNB_IDLE_V2_SPIKE=0" python tools/run_host_tests.py  # motifs longos de S2.4
```

`tools/run_host_tests.py` foi generalizado para compilar todos os `.c` de
um componente (não só `<componente>.c>`) e aceitar `NB_HOST_TEST_CFLAGS`
como defines extras — necessário pra `nb_breath.c`/`nb_attention.c`
conviverem com `idle_engine.c` no mesmo componente e pra rodar a suíte
inteira (incluindo o invariante X→IDLE do `tiny_fsm`) nas duas configs de
flag sem duplicar scripts.

**Turing de mesa confirmado pelo usuário (2026-07-06) — `GO`.** Passo 0
fechado; ver `docs/ROADMAP.md` "Plano S3.7 completo (pós-go)" pra a
sequência do restante da S3.7.

## S3.7 completo — item 1: motor de postura

**`nb_posture.c/.h`** — FSM `HOLD⇄TRANSITION`: baseline que deriva a cada
30-90s (transição ~400ms ease-out) pra uma nova micro-pose (roll ≤0.03,
gaze offset ≤0.05, assimetria ≤0.03 — amplitudes práticas, não os
literais do RFC, mesmo espírito de `NB_IDLE_DRIFT_AMPLITUDE`) que vira o
novo repouso — nunca retorna à pose exata. Mesmo padrão de núcleo puro do
`nb_attention.c` (xorshift32, alvo re-sorteado + suavização exponencial).

Soma-se em `compute_output()` sob `NB_IDLE_V2_SPIKE`: `roll` → `tilt`,
`gaze_x/y` → `gaze_x/y`, `asymmetry` → `width_l`/`width_r` (diferencial,
campo ocioso sob o spike já que `CURIOUS_TILT` fica atrás de
`!NB_IDLE_V2_SPIKE`). Tickada incondicionalmente junto da atenção.

`nb_idle_engine_reset_transient()` (novo, público) zera a postura de
volta ao centro (invariante H7) — gancho exposto, **ainda não chamado por
nenhuma casca**: liga isso a um evento real de `tiny_fsm`/`main.c` fica
pro item 3 ("acoplamentos") do "Plano S3.7 completo".
