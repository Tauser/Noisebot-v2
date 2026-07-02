# NoiseBot 2 — Comportamento: Estados, Emoções e Arbitragem

Especificação do sistema autonômico (L4). Implementação: `tiny_fsm`,
`emotion_core`, `reflex_engine`, `idle_engine`, `schedule_core` — todos pelo
padrão núcleo/casca (P3), com host-test. Herança: state_machine/emotion_model/
behavior_engine do v1, reescritos.

## 1. Máquina de estados (tiny_fsm)

8 estados. O v1 tinha 11; MEDITATION, SILENT_COMPANY e MAINTENANCE viram
**modos parametrizados**, não estados (eliminam transições duplicadas — a
fonte da classe de bug mais recorrente do v1).

| Estado | Entrada por | Saída para | Base visual |
| --- | --- | --- | --- |
| `BOOT` | reset | IDLE (boot ok) ou SAFE_MODE (falha crítica) | tela neutra estática |
| `IDLE` | qualquer estado ao terminar | ATTENTIVE, RESPONDING, TOUCH_REACTING, SLEEPING | **baseline** (ver §2) |
| `ATTENTIVE` | wake word, presença, som saliente | RESPONDING (fala), IDLE (timeout 8–15 s) | NEUTRAL + motifs de atenção |
| `RESPONDING` | turno de conversa (SAY_* ativo) | IDLE (fim/cancel), ATTENTIVE (follow-up) | expressão do turno + boca ativa |
| `TOUCH_REACTING` | touch (TAP/LONG/SUSTAINED) | IDLE (fim da reação) | reação afetiva (§4) |
| `SLEEPING` | circadiano, inatividade longa, comando | IDLE (wake/toque/hora) | olhos fechados, respiração lenta |
| `ERROR` | fault recuperável | IDLE (recuperação) | expressão ALARMED + ícone |
| `SAFE_MODE` | falha de boot/safety pegajosa | somente reset | face mínima estática + ícone |

**Modos de IDLE** (flags, não estados): `quiet` (ex-MEDITATION: sem voz
proativa, mic bloqueado, motifs lentos), `companion` (ex-SILENT_COMPANY: após
~2 h sem interação — presença mínima, sem iniciativa), `maintenance`
(diagnóstico via dashboard, timeout 5 min).

### 1.1 Invariante central (gate H7 — testado, nunca prometido)

**Toda transição X→IDLE** produz: `expr==NEUTRAL`, `gaze==(0,0)`,
`pescoço==centro` (quando existir), `overlays transitórios==∅`, `LED==idle`,
`boca==fechada`. Host-test table-driven cobre 100% das transições ×
modos e roda em todo push. Entradas em IDLE **primeiro limpam, depois aceitam**
novos comportamentos.

### 1.2 Regras de transição

- Safety vence tudo: `EVENT_FAULT` → ERROR/SAFE_MODE de qualquer estado.
- Touch vence fala: toque em RESPONDING dispara barge-in físico
  (cancela SAY, vai a TOUCH_REACTING).
- Wake é ignorado em SLEEPING profundo? Não — wake acorda (SLEEPING→ATTENTIVE),
  exceto em modo `quiet`.
- Queda do server em RESPONDING: fade de áudio ≤ 300 ms → IDLE (nunca
  congela em RESPONDING).
- Timeout é transição, não exceção: todo estado não-IDLE tem deadline
  documentado no header do componente.

## 2. Modelo emocional (emotion_core)

Vetor contínuo 2D, herdado do v1 (validado em produto):

```
valência  ∈ [-1, +1]   aversão/desprazer  →  prazer/calor
ativação  ∈ [-1, +1]   calmo/sonolento    →  excitado/alerta
```

**Dinâmica:** estímulos somam deltas (clamp em ±1); decaimento exponencial
para (0,0) com constante ~60 s até <5% do pico; o vetor mapeia por
nearest-neighbor às âncoras das 10 expressões (§3 de `VISUAL.md`); mudança de
expressão mapeada → `face_service` com transição suave (~220 ms).

**Fontes de estímulo:**

1. **Reflexos locais** (tabela fixa em `reflex_engine`, funciona offline) —
   herdada dos 13 eventos do v1:

| Estímulo local | Δvalência | Δativação | Nota |
| --- | --- | --- | --- |
| TOUCH_TAP | + pequeno | + pequeno | calor breve |
| TOUCH_LONG | ++ | + | calor maior |
| TOUCH_WARM_PULSE (sustained 3–8 s) | ++ | 0 | pulso periódico |
| TOUCH_DEEP (>8 s) | +++ | − | relaxamento |
| TOUCH_CARESS (>15 s) | ++++ | −− | satisfação máxima |
| VOICE_START (wake) | + | ++ | atenção |
| VOICE_LOUD | − | +++ | sobressalto |
| VOICE_SOFT | + | + | curiosidade |
| AUDIO_PLAYING | + | + | engajamento |
| ENTERING_SLEEP | 0 | −−− | |
| WAKING_UP | + | + | |
| MOTION_FAULT | −−− | +++ | alarme |
| IDLE_LONG (sozinho ~30 min) | − | − | tristeza suave → alimenta boredom |

   (Valores exatos são `#define` do núcleo, ajustados em bancada e travados
   por host-test de regressão.)

2. **Estímulos da mente** (`STIMULUS{valence, arousal, tag}` via NBP/2) — o
   server traduz conteúdo semântico ("o usuário elogiou", "notícia triste")
   em vetor. O firmware **integra**, nunca obedece cegamente: o delta é
   clampeado por tick e o decaimento continua valendo. `EXPRESSION_HINT` é
   sugestão pontual com duração; expira sozinha e nunca sobrevive à entrada
   em IDLE.

**Persistência:** o vetor não persiste; a última expressão discreta persiste
em NVS (restaurada como âncora inicial no boot, exceto negativas — boot
nunca acorda ALARMED/ANGRY: âncora clampeada a neutral/positiva).

## 3. Arbitragem (reflex_engine)

Uma única tabela de prioridade decide quem controla face/corpo a cada tick —
ninguém "pega" o display por conta própria:

```
P0 safety/erro  >  P1 touch reflexo  >  P2 fala (SAY)  >
P3 hint da mente  >  P4 emoção mapeada  >  P5 motifs de idle  >  P6 baseline
```

Regras: camada superior ativa suprime as inferiores **sem destruí-las** (ao
terminar, a inferior reaparece — ex.: hint expira e a emoção mapeada volta);
blink e micro-drift de gaze atravessam P2–P6 (só safety/touch os pausam);
conflitos dentro da mesma prioridade: o mais recente vence.

## 4. Reação a toque

Touch é vínculo afetivo, nunca comando de voz. Semântica herdada do v1 2.2A:
TAP (< 400 ms) → reação curta feliz; LONG (0.4–3 s) → carinho; SUSTAINED
(3 s+) → sequência de carinho com pulsos de calor a cada ~4 s; release →
retorno suave. Resposta visível < 80 ms p95 (budget de `QUALITY.md`).

## 5. Circadiano, tédio e agenda

- `idle_engine` mantém relógio circadiano (hora via TIME_SYNC; fallback:
  contador local + último horário NVS): DAWN suaviza despertar, NIGHT reduz
  brilho/frequência de motifs e favorece SLEEPING.
- Tédio: sem interação, IDLE_LONG alimenta valência negativa lenta; após
  ~2 h ativa modo `companion`; interação zera.
- `schedule_core`: timers/alarmes locais persistidos em NVS; disparo publica
  evento → reflexo (expressão + LED + som local) e `EVENT_TIMER_FIRED` à
  mente (que pode falar a respeito, se conectada).

## 6. O que o comportamento NÃO faz

- Não fala nem decide conteúdo (mente).
- Não move servo fora de `motion_safety` ARMED.
- Não muda de estado por mensagem da mente diretamente — mensagens viram
  eventos/estímulos que a FSM avalia (a mente influencia, o corpo decide).
- Não tem estado escondido: todo estado/modo aparece em `STATUS` e no
  dashboard.
