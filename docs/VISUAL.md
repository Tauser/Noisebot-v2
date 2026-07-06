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
completo + âncora no plano emocional (valência, ativação) usada pelo
nearest-neighbor do `emotion_core`:

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

## 3. Vida em IDLE (idle_engine)

O baseline não é estático — é **movimento mínimo vivo**. Desde a S3.7
(`NB_IDLE_V2_SPIKE` ligada por padrão, `GO` confirmado em 2026-07-06,
`docs/RFC-VIDA-V2.md` §7), o modelo em produção é o abaixo; o catálogo de
motifs de S2.4 (segunda tabela) só roda na config legada
`NB_IDLE_V2_SPIKE=0`, mantida por enquanto pra regressão de host-test.

**Modelo atual (`NB_IDLE_V2_SPIKE=1`, padrão):**

| Motor | Núcleo | Descrição |
| --- | --- | --- |
| Respiração | `nb_breath.c` | Fator 1±2% (período ~6 s) sobre `open_l`/`open_r`, aplicado depois do blink. |
| Atenção | `nb_attention.c` | FSM `FIXATE⇄SACCADE`: fixação 0.5–3 s com micro-tremor (amplitude 0.06, suavizado, retunado em bancada), sacada 80 ms ease-out pra alvo em `[-0.35,0.35]²`, ~40% de viés de retorno ao centro. Substitui `SOFT_DRIFT`. |
| Postura | `nb_posture.c` | FSM `HOLD⇄TRANSITION`: a cada 30–90 s, deriva ~400 ms pra uma nova micro-pose (roll/gaze offset/assimetria pequenos) que vira o novo repouso — nunca repete a pose exata. Soma-se a `tilt`/`gaze`/`width_l`-`width_r`. |

Blink independente (Poisson, mesmos parâmetros da tabela legada abaixo)
continua ativo nas duas configs. O restante do RFC §7 (motor de energia,
acoplamentos blink×sacada, LED em fase com a respiração, gestos
`CHECK_IN`/`SLOW_BLINK`/`SIGH`) ainda não está implementado — ver "Plano
S3.7 completo" em `docs/ROADMAP.md` pro estado de cada item.

**Catálogo legado (`NB_IDLE_V2_SPIKE=0`, S2.4)** — calibrado contra o EMO
(v1 `IDLE_REFERENCE.md`, análise por olho de vídeo real):

| Motif | Descrição | Duração | Distribuição IDLE |
| --- | --- | --- | --- |
| `SOFT_DRIFT` | micro-jitter de gaze, amplitude ≤ 0.04 | contínuo | sempre |
| `BLINK_BAR` | blink rápido 80–120 ms (barra) | ~100 ms | Poisson, média 5 s (min 1.8 s) |
| `DOUBLE_BLINK` | dois blinks com gap 400–1000 ms | ~1.5 s | ~30% dos blinks |
| `CURIOUS_TILT` | um olho +30% largura sustentado, blink preserva forma | 3–5 s | 30% dos motifs |
| `HEAD_TILT_HOLD` | assimetria vertical ±0.10 sustentada (roll visual) | 5–15 s | 20% |
| `LOOK_DOWN_BLINK` | gaze +0.4 ↓ → blink → hold 700 ms → blink → retorno | ~2 s | 15% |
| `LINE_BLINK` | blink simples isolado | ~100 ms | 15% |
| `SIDE_PEEK` | olhada lateral com retorno | ~1.5 s | 10% |
| `VERTICAL_SCAN` / `CROSS_SCAN` | varredura vertical/diagonal | ~2 s | 5% + 5% |

Motifs longos a cada 15–40 s em IDLE; em ATTENTIVE, 5–13 s com peeks/scans
mais frequentes. Modo `quiet`/NIGHT: frequências ÷2, brilho reduzido.

**Critério de aceite (gate S2.4, sessão de 60 s, só se aplica à config
legada):** ≥1 expressão sustentada (CURIOUS_TILT ou HEAD_TILT_HOLD); ≥1
LOOK_DOWN_BLINK ou double blink; ≥2 blink bars; drift ≤ 0.04; nenhum
intervalo de 15 s com os olhos idênticos.

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
- Paridade v1: o gate S2.2 compara lado a lado com o robô v1 rodando — a
  personalidade visual é um ativo do produto e não regride.
