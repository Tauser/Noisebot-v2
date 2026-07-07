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

O blink independente (Poisson) **não mudou** com o spike. `main.c`
continua chamando `nb_idle_engine_tick()` sem nenhuma alteração — a troca
de comportamento foi inteira interna ao núcleo, preservando "zero
mudança fora do idle_engine" do escopo do spike. **Nota (item 8,
2026-07-07):** a flag `NB_IDLE_V2_SPIKE` e o catálogo antigo que ela
alternava foram removidos por completo depois que o spike fechou `GO` e
os itens 1-7 do "Plano S3.7 completo" ficaram prontos — a suíte roda
numa config só agora (`python tools/run_host_tests.py`), sem o par de
configs descrito abaixo (mantido como registro histórico do processo do
spike).

`tools/run_host_tests.py` foi generalizado para compilar todos os `.c` de
um componente (não só `<componente>.c>`) e aceitar `NB_HOST_TEST_CFLAGS`
como defines extras — necessário pra `nb_breath.c`/`nb_attention.c`
conviverem com `idle_engine.c` no mesmo componente; durante o spike isso
também rodou a suíte inteira (incluindo o invariante X→IDLE do
`tiny_fsm`) nas duas configs de flag sem duplicar scripts.

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

Soma-se em `compute_output()`: `roll` → `tilt`, `gaze_x/y` → `gaze_x/y`,
`asymmetry` → `width_l`/`width_r` (diferencial). Tickada
incondicionalmente junto da atenção.

`nb_idle_engine_reset_transient()` (novo, público) zera a postura de
volta ao centro (invariante H7) — gancho exposto, **ainda não chamado por
nenhuma casca**: liga isso a um evento real de `tiny_fsm`/`main.c` fica
pro item 3 ("acoplamentos") do "Plano S3.7 completo".

## S3.7 completo — item 2: motor de energia

**`nb_energy.c/.h`** — sem RNG (ao contrário dos outros motores): integra
três sinais de entrada num nível contínuo de sonolência `level` ∈ [0,1]
via suavização exponencial (tau 5s, sem saltos em degrau):

- **Tédio** (`boredom_ms`, ms desde o último estímulo acima de
  `BASELINE`/`IDLE_MOTIF`) — sobe ao teto depois de ~5 min sem estímulo.
- **Ativação** (`arousal` ∈ [-1,1], vetor do `emotion_core`) — só a
  componente positiva reduz sonolência (tristeza/raiva não "acordam" o
  robô).
- **`quiet_mode`** (já existente, reaproveitado) — viés fixo de
  sonolência à noite/circadiano.

Efeito em `compute_output()`/`sample_next_blink_ms()`: pálpebra de
descanso multiplicada por `1 - level*0.25` (nunca fecha de vez — isso é
visual de `SLEEPING`, outro estado); intervalo médio/piso do blink
independente multiplicado por `1 + level*1.0` (até 2× mais espaçado no
teto).

`nb_idle_engine_set_energy_inputs(engine, boredom_ms, arousal)` (novo,
público) alimenta os dois sinais que o núcleo não pode calcular sozinho
— **ainda não chamado por nenhuma casca**: sem essa chamada, o robô nunca
fica sonolento visivelmente (defaults 0/0.0). A fiação real (tédio vindo
de `reflex_engine`, ativação vindo de `emotion_core`) continua em aberto,
sem item específico do plano ainda assinado pra ela — o item 3 abaixo
cobriu os três acoplamentos que o RFC lista explicitamente, não essa
fiação de entrada (mesma exceção documentada no `ROADMAP.md`, e mesmo
assim não foi necessária pro que o item 3 cobriu: os sinais entram por
parâmetro, sem tocar `circadian_core`).

## S3.7 completo — item 3: acoplamentos + blink unificado

Três acoplamentos do RFC §7, todos sobre a mesma base já construída nos
itens 1-2:

- **Blink×sacada.** `nb_idle_engine_init()` registra
  `nb_attention_set_saccade_callback()` (exposto no spike, sem uso até
  agora) apontando pra `on_attention_saccade()`: quando uma sacada
  começa, dispara `start_blink_motif()` de verdade — mas só se o slot
  estiver livre (`active_motif == NONE`), preservando a exclusividade já
  existente. Como `start_blink_motif()` já resorteia
  `next_blink_at_ms` no final, o blink independente (Poisson) e o
  disparado por sacada caem no mesmo agendador sem precisar de mecanismo
  novo — a "fusão" que o RFC pede.
- **Roll segue gaze com ~100ms de atraso.** Novo campo
  `roll_gaze_lag_x` (filtro passa-baixa do `drift_x` já resolvido, tau
  100ms), somado ao `tilt` com ganho pequeno (0.15) — sutil, cabeça
  acompanha o olhar sem copiar o valor inteiro. Também é estado
  transitório: `nb_idle_engine_reset_transient()` zera junto com a
  postura (invariante H7).
- **Respiração em fase com o LED idle.** Único ponto que sai do
  `idle_engine`: `nb_idle_output_t` ganha `breath_scale` (o mesmo fator
  que já modula `open_l`/`open_r`); `main.c` multiplica o brilho do LED
  por esse valor, mesmo clock, sem estado duplicado — LED e respiração
  literalmente compartilham o sinal, não só o período.

Host-tests: maioria das transições `FIXATE→SACCADE` do motor de atenção
dispara um blink (tolerância pra quando o slot já está ocupado); o filtro
de roll nunca sai do envelope do gaze e nunca copia o valor instantâneo
(prova que é atraso, não passagem direta). Suíte inteira verde; build
limpo — confirmação visual da fase (LED×respiração) fica pra bancada.

**Achado de bancada (2026-07-06, não corrigido ainda):** com o blink×sacada
disparando bem mais que o blink Poisson sozinho, ficou visível um artefato
raro no renderer (linhas diagonais/buracos nos cantos dos olhos durante a
piscada) que provavelmente já existia mas era raro demais pra notar antes.
Analisado o cálculo de curvatura (`nb_face_core_eye_column`) e o retângulo
de flush (`eye_dirty_rect`/`nb_display_hal_rect_union`) sem achar a causa
raiz por leitura estática -- fica registrado pra investigação futura
(fora do escopo do `idle_engine`; usuário decidiu não instrumentar agora).

## S3.7 completo — item 4: gestos nomeados

Três gestos (RFC §7), reaproveitando o mesmo mecanismo de slot exclusivo
(`active_motif`) do blink -- sem núcleo novo, só 3 casos a mais no
`switch` de `compute_output()` e 3 agendadores independentes (mesmo
padrão do blink Poisson):

- **`CHECK_IN`** ("gaze à frente + micro-abertura + blink", ~1×/1-3min,
  frequência literal do RFC): puxa o gaze pro centro no pico do gesto,
  abre os olhos um pouco mais (+8%), fecha rápido perto do fim.
- **`SLOW_BLINK`** (contentamento): fecha e reabre suavemente (onda
  senoidal, ~650ms) — bem mais lento que o blink normal (80-120ms,
  corte instantâneo).
- **`SIGH`** (acomodação): gaze desce e volta suave (~1.8s), bem mais
  sutil que `LOOK_DOWN_BLINK` (amplitude 0.15 vs 0.7 -- sem blink, sem
  hold).

`SLOW_BLINK`/`SIGH` não têm frequência no RFC — intervalos práticos
(30-90s / 45-120s), mesma classe de retune-em-bancada já usada nas
amplitudes do motor de atenção. `quiet_mode` dobra os três intervalos,
mesma regra do blink. Host-tests: os três disparam dentro da janela
simulada; `quiet_mode` reduz a contagem total. Suíte inteira verde;
build limpo — confirmação visual (a "textura" de cada gesto) fica pra
bancada.

## S3.7 completo — item 8: aposentar o catálogo antigo (S2.4)

Com os itens 1-7 fechados, o modelo de 3 motores deixou de ser um spike
opcional e virou o único comportamento com sentido em produção — a flag
`NB_IDLE_V2_SPIKE` (que alternava entre ele e o catálogo de motifs
sorteados de S2.4) foi removida por completo do código, não só desligada
por padrão.

Removido de `idle_engine.h`: o `#define NB_IDLE_V2_SPIKE`, os motifs
`CURIOUS_TILT`/`HEAD_TILT_HOLD`/`LOOK_DOWN_BLINK`/`LINE_BLINK`/
`SIDE_PEEK`/`VERTICAL_SCAN`/`CROSS_SCAN` do enum `nb_idle_motif_t`, os
campos de métrica que só eles incrementavam (`sustained_count`,
`look_down_count`, `side_peek_count`, `scan_count`, `line_blink_count`),
e os campos de estado do `SOFT_DRIFT` legado (`next_motif_at_ms`,
`drift_target_x/y`, `drift_next_target_at_ms`).

Removido de `idle_engine.c`: `pick_long_motif()` (distribuição ponderada
dos motifs longos), `start_long_motif()`, `sample_next_motif_ms()`, os
`case` de `compute_output()` que desenhavam esses motifs, o ramo `#else`
de `nb_idle_engine_tick()` que rodava o `SOFT_DRIFT` por passeio
aleatório, e as constantes de amplitude (`NB_IDLE_DRIFT_AMPLITUDE` e
similares) que só esses motifs usavam.

Removido de `test_idle_engine.c`: os três testes que só faziam sentido
no catálogo antigo (agendamento mais frequente em `ATTENTIVE`, critério
de aceite de `VISUAL.md` §3 original, `CURIOUS_TILT` alargando um olho
só) — os demais testes perderam o `#if`/`#endif` de config alternativa
(viraram incondicionais, já que só existe um caminho agora).

`nb_posture.h`'s `NB_POSTURE_ASYMMETRY_AMPLITUDE` deixou de ser "campo
ocioso sob o spike": `width_l`/`width_r` sempre recebem a contribuição da
postura agora, já que não existe mais nenhuma config onde ela fica sem
consumidor.

Suíte inteira verde numa config só (23 componentes -- não há mais uma
"config antiga" separada pra rodar); build limpo. `docs/VISUAL.md` §3/§7
e `docs/BEHAVIOR.md` §2 atualizados (ver `docs/ROADMAP.md` item 8 do
"Plano S3.7 completo" pra evidência completa).
