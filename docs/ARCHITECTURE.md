# NoiseBot 2 — Arquitetura

## 1. Topologia

```
┌──────────────────────────────┐        WiFi (LAN local)        ┌──────────────────────────────┐
│  ROBÔ — Freenove S3 N16R8    │◄──────── NBP/2 (TCP) ─────────►│  MENTE — server Python (PC)  │
│  corpo + autonômico          │                                │  cognição                    │
└──────────────────────────────┘                                └───────────▲──────────────────┘
                                                                            │ HTTP/WS
                                                                 ┌──────────┴──────────┐
                                                                 │  Dashboard React     │
                                                                 └─────────────────────┘
```

**Regra da mesa** (fronteira corpo/mente):

- Acontece com o robô sozinho na mesa → **firmware**: idle vivo, blink, gaze
  errante, emotion decay, reação a toque, circadiano, sono, timers, expressões,
  safety.
- Precisa de linguagem, memória ou raciocínio → **server**: conversa, persona
  profunda, reconhecimento de rosto, tools, resumos, agenda cognitiva.
- Server → robô: **estímulos e intenções semânticas** (`STIMULUS`,
  `EXPRESSION_HINT`, `SAY`), nunca frames nem posições cruas de servo.
- Robô → server: **eventos tipados e mídia sob demanda** (áudio de sessão,
  JPEG por snapshot).
- Autoridade de tempo: o robô carimba seus eventos com clock monotônico local;
  o server converte via `TIME_SYNC`. Decisões de comportamento autonômico nunca
  dependem de relógio do server.

## 2. Camadas do firmware (5)

```
┌────────────────────────────────────────────────────────────┐
│ L4 AUTONÔMICO   reflex_engine, idle_engine, emotion_core,  │
│                 tiny_fsm, schedule_core                    │
├────────────────────────────────────────────────────────────┤
│ L3 SERVIÇOS     face_service, audio_service, motion_service│
│                 led_service, touch_service, camera_service,│
│                 storage_service, mind_link                 │
├────────────────────────────────────────────────────────────┤
│ L2 SAFETY       motion_safety (veto), power_monitor        │
├────────────────────────────────────────────────────────────┤
│ L1 INFRA        event_bus, config (NVS), logger, watchdog, │
│                 boot_manager, ota                          │
├────────────────────────────────────────────────────────────┤
│ L0 HAL          display, audio_i2s, servo_1wire, led_rmt,  │
│                 touch, camera, sdmmc, wifi                 │
└────────────────────────────────────────────────────────────┘
```

Regras de chamada (idênticas ao v1, que funcionaram):

- Camada N chama apenas N-1 ou inferior.
- Comunicação entre camadas não adjacentes: **sempre event bus**.
- HAL nunca publica no event bus (o serviço dono publica).
- Callbacks do `mind_link` nunca chamam o autonômico diretamente: publicam
  eventos.

## 3. Padrão obrigatório: núcleo funcional, casca imperativa (P3)

Todo componente com lógica não-trivial tem esta forma:

```
components/autonomic/emotion_core/
├── emotion_core.c / .h      # C17 puro. Sem FreeRTOS, sem esp_*, sem malloc.
│                            #   API: init(cfg), tick(now_ms, inputs) → outputs
├── shell/emotion_task.c     # casca: task, filas, clock real, publish no bus
└── host_test/test_emotion_core.c   # roda no host, clock injetado, CI
```

O que isso compra: FSM, emoção, idle, protocolo, safety e schedule testáveis
em milissegundos no host, determinísticos, sem hardware. O `nb_link_engine` do
v1 provou o padrão; no v2 ele é obrigatório (gate de review).

## 4. O autonômico (L4)

| Motor | Responsabilidade | Origem |
| --- | --- | --- |
| `tiny_fsm` | 8 estados: BOOT, IDLE, ATTENTIVE, RESPONDING, TOUCH_REACTING, SLEEPING, ERROR, SAFE_MODE. MEDITATION/SILENT_COMPANY do v1 viram *modos parametrizados* de IDLE | reescrita do state_machine v1 |
| `emotion_core` | vetor valência×ativação, decaimento, integração de estímulos | port do emotion_model v1 |
| `idle_engine` | micro-behaviors, blink Poisson, gaze errante com saccades, tédio, circadiano | fusão idle/boredom/circadian v1 |
| `reflex_engine` | mapa estímulo→reação (toque→afeto, som alto→sobressalto, wake→atenção) | novo (extraído do behavior_engine v1) |
| `schedule_core` | timers, alarmes, lembretes locais com persistência NVS | novo |

**Invariante central (testada, não prometida):** toda transição X→IDLE deixa
`expr==NEUTRAL && gaze==centro && pescoço==centro && overlays==∅ && led==idle`.
Host-test table-driven cobre 100% das transições e roda em todo push. Esta é a
lição H7 do v1: a classe de bug mais recorrente vira gate permanente.

O server alimenta o autonômico com `STIMULUS{valence, arousal, tag}` e
`EXPRESSION_HINT`; nunca o substitui. Queda do server no meio de uma ação:
terminar o gesto, fade de áudio ≤ 300 ms, baseline IDLE, ícone de mente offline.

## 5. Render

- Renderer paramétrico C++ (LovyanGFX) isolado em
  `services/face/renderer/` atrás de `extern "C"` — referência direta: o
  renderer do head v1 (interpolação 220 ms, blink autônomo, EMA de gaze).
- Double buffer 320×240×16bpp em PSRAM; SPI 50 MHz DMA; 30 fps alvo, tick 20 ms,
  pinned core 1.
- Nenhum outro componente toca o objeto LGFX; nenhum C inclui header C++.

## 6. Concorrência e memória

Tabela de tasks alvo (validar em S2/S4; prioridades relativas são contrato):

| Task | Core | Prio | Nota |
| --- | --- | --- | --- |
| wdog | 0 | 24 | TWDT |
| safety | 1 | 23 | 20 Hz; nunca preemptada por aplicação |
| motion | 1 | 20 | interpolação + heartbeat p/ safety |
| audio_io | 0 | 18 | I2S duplex DMA |
| wake/vad | 0 | 16 | ESP-SR |
| mind_link | 0 | 12 | NBP/2 TX/RX |
| autonomic | 1 | 10 | fsm+emotion+idle tick 20 Hz |
| touch | 0 | 10 | poll 50 Hz |
| render | 1 | 8 | 30 fps |
| led | 0 | 6 | RMT |
| storage | 0 | 4 | SD worker assíncrono |
| camera | 0 | 3 | JPEG sob demanda |

Regras de memória:

- Nenhum framebuffer em SRAM (sprites e câmera → PSRAM). I2S DMA → SRAM.
- Nenhum `malloc` em ISR, safety ou render loop; pools estáticos.
- Event bus: pool estático com slots reservados de safety, fila de safety
  imune a backpressure, ring de auditoria com dump **também em panic** (v1 só
  dumpava em shutdown limpo).
- Escrita SD sempre via worker de baixa prioridade; crash dump é a única
  exceção síncrona.
- Budgets numéricos e enforcement: `HARDWARE.md` §3 e `QUALITY.md`.

## 7. A mente (server)

Cinco atores sobre o event bus interno; **nenhum ator chama outro** — só bus:

| Ator | Responsabilidade |
| --- | --- |
| `TurnEngine` | FSM de conversa: wake→listen→STT→intent/LLM→resposta; barge-in; deadlines |
| `MindOutput` | TTS (Piper), sentencização, scheduling de fala, cancelamento |
| `VisionMind` | snapshot→detecção→identificação→presença/gaze; enrollment |
| `PersonaMind` | perfil, humor cognitivo, memória longa (SQLite), estímulos para o robô |
| `SkillHost` | tools: agenda, busca web (opcional), device commands, fatos |

Providers (STT/LLM/TTS) com circuit breaker, herdados do v1. SQLite é fonte de
verdade de conversas/memória/perfil. I/O bloqueante nunca no event loop
(`asyncio.to_thread`). Segurança operacional: `SECURITY.md`.

## 8. Protocolo

Ver `PROTOCOL.md`. Resumo: um único protocolo (NBP/2) sobre TCP, framing
binário + payload CBOR gerado de `protocol/nbp2.yaml`, token obrigatório,
sequence idempotente, ACK ≠ commit, canal de mídia separado do controle.

## 9. Anti-padrões banidos (com a cicatriz v1 correspondente)

| Anti-padrão | Cicatriz |
| --- | --- |
| Constantes de protocolo duplicadas à mão em duas linguagens | HELLO em string C + dict Python, drift sem alarme |
| Teste de contrato por regex sobre fonte | 20 testes mortos em silêncio após um `git mv` |
| CI com filtro de paths para testes cruzados | idem |
| God-file de borda (HTTP conhecendo semântica de todas as camadas) | `web_service.c` 5,2k LOC |
| Cognição em C embarcado | behavior_engine 1k LOC + iteração por flash |
| Segundo dono de qualquer verdade "temporariamente" | render duplo main/head por meses |
| Segurança como programa futuro | H1–H3 diagnosticados e engavetados |
