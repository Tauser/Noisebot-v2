# NoiseBot 2 — Voz: pipeline do corpo

Especificação do lado firmware da voz (captura, wake, sessão, playback). O
lado da mente (STT/LLM/TTS/turno) está em `SERVER.md` §3–4; o contrato de
mensagens em `PROTOCOL.md`. Herança: invariantes do Voice Audio v2 do v1,
conquistadas em bancada — **não relaxar sem medição**.

## 1. Hardware e formatos

- I2S full-duplex 16 kHz mono (INMP441 + MAX98357A, BCLK/WS compartilhados —
  `HARDWARE.md`). Buffers DMA em SRAM; ring de playback em PSRAM.
- Formato base: PCM16 16 kHz. Opus é capability opcional negociada no HELLO
  (entra só se a medição de CPU do S4 justificar — P5).
- Sem AEC por contrato de placa (1 mic, sem canal de referência do speaker):
  half-duplex conversacional com barge-in por VAD durante fala (grace
  anti-eco) — herdado do v1.

## 2. As três responsabilidades (separação do v1, mantida)

| Responsabilidade | Dono | Regra |
| --- | --- | --- |
| Ativação de intenção | `wake_service` (WakeNet) | só wake word abre sessão de escuta |
| Decisão de fala na sessão | VAD ESP-SR | início de streaming, fim por silêncio, timeout |
| Diagnóstico/ambiente | análise local (RMS/ZCR) | alimenta reflexos e calibração; **nunca** abre sessão |

## 3. Invariantes de sessão (viram host-tests em S4)

- **V-1** Touch é vínculo afetivo; **nunca** abre sessão de voz.
- **V-2** `LISTEN_START` só ocorre dentro de sessão aberta por wake word.
- **V-3** VAD heurístico fora de sessão não publica atividade de voz no bus.
- **V-4** `LISTEN_END` só ocorre se a sessão enviou start **e** áudio
  (nunca END órfão).
- **V-5** Se o VAD ESP-SR estiver indisponível, o listening conversacional
  **não** cai silenciosamente para heurística — fallback é opção explícita
  de bancada, nunca produção.
- **V-6** Sessão sem mente conectada nem intent local aplicável: encerra com
  feedback honesto (expressão + ícone), sem pendurar.

## 4. Parâmetros de sessão (baseline v1 — ajuste só com medição)

| Parâmetro | Valor |
| --- | --- |
| Fim por silêncio | 900 ms |
| Fala máxima por turno | 9,2 s |
| Utterance mín./máx. | 8k / 192k amostras |
| Chunk de streaming | 256 amostras PCM16 |
| Grace de barge-in pós-início de fala | ~0,9 s |
| Wake → overlay listening | < 250 ms p95 (budget) |

## 5. Playback (SAY_*)

- `SAY_BEGIN{turn_id}` → prebuffer no ring PSRAM → I2S; chunks `SAY_AUDIO`
  alimentam o ring; underrun encerra com fade curto + diagnóstico (nunca
  bloqueia a task I2S).
- `SAY_CANCEL` (barge-in/touch): fade ≤ 300 ms, descarta ring, evento à
  mente. Queda de link durante fala: idem, depois baseline IDLE.
- Boca por visemas da amplitude + pre-speech 0,3 s + post-speech settle
  (`VISUAL.md` §4). LED modulado pela fala (`VISUAL.md` §6).
- Sons locais críticos (wake ack, erro, timer) vivem em flash — tocam sem
  mente e sem SD.

## 6. Coexistência com o resto do corpo

- Prioridades de task: audio_io (18) > wake/vad (16) > mind_link (12) >
  render (8) — voz nunca disputa com safety (20+).
- O gate S0.3/S4.1 prova zero underrun com render + SD ativos; esse número é
  o contrato de regressão.
- Modo `quiet` (BEHAVIOR §1): WakeNet suspenso, mic bloqueado, ícone no
  trilho — estado visível, nunca silencioso.
