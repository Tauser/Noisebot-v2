# NoiseBot 2 — Linguagem Visual: Face, Expressões, Overlays e LEDs

Especificação da apresentação. Implementação: `face_service` (+ renderer C++
LovyanGFX atrás de `extern "C"`), `led_service`, arbitragem em
`reflex_engine` (`BEHAVIOR.md` §3). Referência de implementação: renderer do
head v1 (DM2) e calibração de idle contra vídeo do EMO (`IDLE_REFERENCE.md`
do v1).

## 1. Face paramétrica

A face é **estado paramétrico contínuo**, nunca bitmaps trocados:

```
nb_face_state_t (por olho L/R):
  largura, altura, raio de canto, posição (x, y),
  curvatura superior/inferior (tl/tr/bl/br), abertura de pálpebra
+ boca: abertura, curvatura, visema (fala)
+ gaze: (x, y) ∈ [-1, +1]² com EMA
+ roll da face (head tilt visual, ±3°)
```

- Transições interpolam **220 ms** ease-out (validado v1/DM2).
- Render 30 fps, tick 20 ms, double buffer PSRAM, AA sub-pixel
  coluna-a-coluna (técnica do v1).
- Display lógico 320×240 landscape; olhos ocupam o terço central; margens
  inferiores reservadas a texto/boca; trilho de status no topo (§5).

## 2. As 10 expressões-base

Herdadas do v1 (validadas em produto). Cada uma é um `nb_face_state_t`
completo + âncora no plano emocional (valência, ativação).

**Desde a S3.7 completa (item 6, 2026-07-06):** só os 4 hubs
(`NEUTRAL`/`HAPPY`/`SAD`/`ANGRY`) são alcançáveis pelo vetor emocional —
`nb_emotion_core_resolve_face()` faz blend contínuo só entre eles. As
outras 6 âncoras (`CURIOUS/SLEEPY/FOCUSED/SUSPICIOUS/SURPRISED/ALARMED`)
saem de uso (decisão de produto do usuário, diverge do RFC original que
previa mantê-las como fallback nearest-neighbor) — continuam definidas na
tabela abaixo e em `renderer.c` (histórico/referência), mas nada no
firmware as invoca mais. `nb_emotion_core_nearest_expression()` (o
mapeamento antigo, nearest-neighbor sobre as 10) continua existindo por
compatibilidade, sem uso ativo em `main.c`.

| # | Expressão | Âncora (v, a) | Forma característica |
| --- | --- | --- | --- |
| 0 | `NEUTRAL` | (0, 0) | retângulos arredondados ~quadrados (ar≈1.0) |
| 1 | `HAPPY` | (+0.7, +0.3) | curva côncava `⌒⌒` (smile-eyes), fill baixo |
| 2 | `CURIOUS` | (+0.3, +0.4) | um olho ~30% mais largo + leve tilt |
| 3 | `SLEEPY` | (0, −0.8) | barras horizontais finas `— —` |
| 4 | `FOCUSED` | (+0.1, +0.6) | olhos estreitados, gaze fixo |
| 5 | `SUSPICIOUS` | (−0.3, +0.2) | assimetria com squint unilateral |
| 6 | `SURPRISED` | (0, +0.8) | olhos altos verticais `❘ ❘` (ar≈0.8) |
| 7 | `SAD` | (−0.6, −0.4) | cantos externos caídos, gaze baixo |
| 8 | `ALARMED` | (−0.5, +0.9) | olhos grandes + tremor sutil |
| 9 | `ANGRY` | (−0.8, +0.6) | cantos internos baixos, squint agressivo |

Regra: boot nunca restaura expressão negativa (clamp em `emotion_core`).
Expressões nunca substituem o baseline de IDLE — são estados transitórios ou
mapeamentos da emoção que decaem para NEUTRAL.

**Boca (S3.7 completo, item 5, RFC §3):** só os 4 hubs (`NEUTRAL`/
`HAPPY`/`SAD`/`ANGRY`) ganharam `mouth_open`/`mouth_curve` não-neutros —
`HAPPY` aberta e sorrindo, `SAD` curva suave pra baixo, `ANGRY` curva
invertida mais firme que `SAD`. As outras 6 âncoras continuam boca
neutra (`0,0`) — e, desde o item 6, nem são mais alcançáveis pelo vetor
de qualquer forma. Sem visemas de fala ainda (S4 voz).

**Emenda §3.1a (2026-07-07, decisão em bancada):** boca é canal de
intensidade, não traço permanente — só existe quando a intensidade
emocional (norma do vetor) cruza o nível médio (aparece em ≥0.40, some
em <0.30, histerese sem flicker; escala contínua até o pico ~0.70).
`NEUTRAL`/`IDLE` nunca mostra boca (repouso do temperamento é `+0.10`,
bem abaixo do limiar) — o micro-sorriso de repouso vive nos *olhos*, não
na boca. Fala e arcos/sequências (bocejo, `SIGH`) ignoram o limiar.
Posição ancorada à face (soma `gaze_shift`/`x_offset` como os olhos,
paralaxe 0.6 no eixo Y), não mais fixa no painel.

**Variantes episódicas (item 7):** 2 por hub, sorteadas ao entrar na
região (troca de hub dominante) e mantidas por todo o episódio —
`NEUTRAL` sereno (pálpebra ~5% mais baixa) / atento (gaze um traço mais
alto); `HAPPY` radiante (boca mais aberta) / contido (sorriso mais
fechado); `SAD` murcho (gaze baixo) / magoado (assimetria); `ANGRY`
irritado (squint parcial) / bravo (squint cheio). Tempera o resultado do
campo contínuo, escalado pelo peso do hub dominante — nunca sai do
envelope da região (clamp).

## 3. Vida em IDLE (idle_engine)

O baseline não é estático — é **movimento mínimo vivo**. Modelo de 3
motores + respiração + blink + gestos nomeados (S3.7 completo,
`docs/RFC-VIDA-V2.md` §7), único caminho desde o item 8 (aposentadoria
do catálogo de motifs de S2.4, `NB_IDLE_V2_SPIKE` removida — deixou de
ser flag opcional).

| Motor | Núcleo | Descrição |
| --- | --- | --- |
| Respiração | `nb_breath.c` | Fator 1±2% (período ~6 s) sobre `open_l`/`open_r`, aplicado depois do blink. |
| Atenção | `nb_attention.c` | FSM `FIXATE⇄SACCADE`: fixação 0.5–3 s com micro-tremor (amplitude 0.06, suavizado, retunado em bancada), sacada 80 ms ease-out pra alvo em `[-0.35,0.35]²`, ~40% de viés de retorno ao centro. |
| Postura | `nb_posture.c` | FSM `HOLD⇄TRANSITION`: a cada 30–90 s, deriva ~400 ms pra uma nova micro-pose (roll/gaze offset/assimetria pequenos) que vira o novo repouso — nunca repete a pose exata. Soma-se a `tilt`/`gaze`/`width_l`-`width_r`. |
| Energia | `nb_energy.c` | Nível contínuo de sonolência [0,1] a partir de tédio + ativação + `quiet_mode`; descansa a pálpebra (até 25% mais fechada) e espaça o blink (até 2×). Entradas reais (tédio/ativação) ainda não ligadas por nenhuma casca — só o núcleo existe. |

**Acoplamentos ligados:** blink×sacada (sacada dispara blink de fato,
respeitando exclusividade de slot); roll segue gaze com ~100ms de atraso
(filtro passa-baixa, ganho sutil); respiração em fase com o LED idle
(`main.c` multiplica o brilho pelo mesmo fator que modula os olhos).
Blink independente segue Poisson (média 5s, mínimo 1.8s, ~30% duplos).

**Gestos nomeados** (mesmo slot exclusivo do blink, sem núcleo novo):
`CHECK_IN` (~1×/1-3min — gaze ao centro + micro-abertura + blink),
`SLOW_BLINK` (fecha/reabre devagar, ~650ms, contentamento), `SIGH` (gaze
desce e volta suave, ~1.8s, acomodação). `quiet_mode` dobra os três.

**Artefato conhecido, não corrigido:** o acoplamento blink×sacada
aumentou muito a frequência de piscadas, expondo um bug raro e
pré-existente no renderer (linhas diagonais/buracos nos cantos do olho
durante a piscada) — causa raiz não confirmada, ver `docs/ROADMAP.md`
item 4 do "Plano S3.7 completo".

Ainda falta a fiação real de tédio/ativação pro motor de energia — ver
"Plano S3.7 completo" em `docs/ROADMAP.md` pro estado de cada item.

**Histórico (S2.4, aposentado 2026-07-07):** o modelo anterior sorteava
motifs discretos (`SOFT_DRIFT` contínuo + `BLINK_BAR`/`DOUBLE_BLINK` via
Poisson + um catálogo de motifs longos — `CURIOUS_TILT`,
`HEAD_TILT_HOLD`, `LOOK_DOWN_BLINK`, `LINE_BLINK`, `SIDE_PEEK`,
`VERTICAL_SCAN`/`CROSS_SCAN` — a cada 15–40s em IDLE, 5–13s em
ATTENTIVE). O gate de aceite original (sessão de 60s: ≥1 expressão
sustentada, ≥1 look-down/double-blink, ≥2 blink bars, drift ≤0.04, sem
intervalo de 15s idêntico) foi superado pelo gate final da S3.7 (item 9)
sobre o modelo novo. Código e host-tests removidos; ver git history
(commits da S3.7 completo, item 8) se precisar consultar os números
originais.

## 4. Sequências compostas

Performances multi-passo (motor de combo do `face_service`), nunca em
idle_engine:

- `WAKE_GREETING` (~4.5 s, referência EMO): SLEEPY 600 ms → SURPRISED 500 ms
  → HAPPY 1.2 s + texto "Hi" → blink bar → HAPPY 700 ms → look-down 600 ms →
  settle NEUTRAL. Dispara em SLEEPING→IDLE deliberado (wake, DAWN).
- `TIMER_FIRED`: SURPRISED + toast do label + LED pulso + som local.
- `ERROR_ENTER`: ALARMED + ícone persistente.
- Fala: boca por visemas a partir da amplitude do TTS (padrão v1), com
  pre-speech (0.3 s de atenção antes do áudio) e post-speech settle.

## 5. Overlays e trilho de status

Camadas de render (baixo→alto): face → boca/visemas → texto/toast → trilho
de status → debug (dev build). Regras herdadas do v1 16.2:

- **Trilho invisível** no topo: só desenha ícones ativos, slots 28×28 px,
  máscaras 1-bit tingidas no draw (SVG é fonte de editor; PBM/header C é
  runtime — nunca parser de imagem no firmware).
- Ícones persistentes: mic ativo, mic bloqueado (modo quiet), fala ativa,
  **mente offline**, SD indisponível, fault de safety, alerta de energia.
- **Toast/mensagem**: uma linha, fonte Montserrat (port do v1), duração
  definida pelo chamador, nunca cobre os olhos.
- Overlay de listening: indicador sutil durante sessão de captura.
- Nenhum serviço desenha diretamente: publica estado; `face_service` compõe.
- Com zero status ativos, nada aparece. IDLE limpo é IDLE limpo.

## 6. Linguagem de LED (led_service)

2× WS2812 (GPIO21) + LED onboard (38, status de sistema, não expressivo):

| Contexto | Padrão |
| --- | --- |
| IDLE | respiração lenta (~6 s), cor quente, brilho circadiano |
| ATTENTIVE/listening | pulso médio, cor fria clara |
| RESPONDING (falando) | modulação pela amplitude da fala |
| TOUCH | flash quente + fade longo |
| SLEEPING | apagado ou mínimo (~2%) |
| ERROR/fault | vermelho intermitente (P0 — nunca suprimido) |
| Timer/alarme | pulso rápido na cor do alerta |

LEDs seguem a mesma arbitragem P0–P6 de `BEHAVIOR.md` §3 — nunca contradizem
a face.

## 7. Contratos com o resto do sistema

- A mente influencia por `STIMULUS`/`EXPRESSION_HINT`/`SAY_*`; nunca desenha.
  Nenhuma API gráfica atravessa NBP/2.
- `tiny_fsm` decide estado; `face_service` apresenta; `reflex_engine` arbitra.
- Invariante de IDLE (`BEHAVIOR.md` §1.1) vale para tudo deste documento:
  entrada em IDLE limpa expressão, gaze, overlays transitórios e LED.
- **Paridade v1 (gate S2.2) aposentada de propósito** (RFC-VIDA-V2.md §12,
  decisão de produto): persona v2 diverge intencionalmente do v1 — o
  side-by-side registrado no gate final da S3.7 (item 9) é o novo
  baseline, não mais uma comparação de regressão contra o v1.
