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
