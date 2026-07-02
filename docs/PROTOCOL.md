# NBP/2 — NoiseBot Protocol v2 (robô ↔ mente)

Um único protocolo substitui os três do v1 (inter-MCU, bridge, HTTP de
controle). O dashboard fala com o server por HTTP/WS; **só o server fala com o
robô**.

## 1. Regra fundadora: schema-first (P4)

- Fonte de verdade única: `protocol/nbp2.yaml`.
- Codegen produz: encoder/decoder C17 (tabelas estáticas, zero malloc) em
  `protocol/generated/c/` e dataclasses Python em `protocol/generated/python/`.
- **Proibido**: editar código gerado; declarar constante de protocolo fora do
  YAML; espelhar structs à mão.
- Golden tests: para cada mensagem, o CI codifica um vetor canônico em C e em
  Python e compara os bytes. Divergência = build vermelho. (Cicatriz v1:
  HELLO duplicado em string C + dict Python, guardado por regex que morreu.)

## 2. Transporte e framing

- TCP, server escuta na porta 9100. Robô é o cliente (reconexão com backoff).
- Frame: `SOF(0xNB=0xAB) | len u16 LE | type u16 | seq u32 | payload CBOR | CRC32`.
- CRC32 (o v1 usava CRC8 no bridge; mídia de voz merece mais).
- Payload máximo 4096 B no canal de controle; mídia usa chunks próprios.
- Dois canais lógicos na mesma conexão: `CTRL` (controle/eventos, sempre
  drenado primeiro) e `MEDIA` (áudio, JPEG) com backpressure independente.
  Eventos de safety/estado nunca aguardam mídia.

## 3. Sessão

1. Robô conecta → envia `HELLO{proto_ver, boot_id, caps, token}`.
2. Server valida token (timing-safe; **obrigatório**, sem modo opt-out) →
   responde `HELLO_ACK{proto_ver, boot_id, caps}` → `TIME_SYNC`.
3. Estado `READY`. Heartbeat 1 s em ambas direções; 3 perdidos → sessão morta,
   robô continua autônomo e reconecta.
4. `boot_id` novo de qualquer lado invalida estado de sessão pendente
   (transfers, turnos, handles). Reconexão exige HELLO completo.

Semântica herdada do melhor do v1 (link engine):

- Comandos mutadores carregam `seq`; retry reutiliza o mesmo `seq`
  (idempotência no receptor).
- ACK confirma recepção, **não** commit de domínio.
- Timeout é resultado **ambíguo** — o chamador reconcilia explicitamente;
  nunca assumir que não executou.

## 4. Mensagens núcleo (v0 — a lista canônica vive no YAML)

| Direção | Mensagem | Conteúdo |
| --- | --- | --- |
| ↔ | `HELLO` / `HELLO_ACK` | versão, boot_id, capabilities, token (robô→server) |
| ↔ | `HEARTBEAT` | contador, uptime |
| S→R | `TIME_SYNC` | unix_ms + monotonic de referência |
| S→R | `STIMULUS` | valence f32, arousal f32, tag enum — alimenta emotion_core |
| S→R | `EXPRESSION_HINT` | expressão id, intensidade, duração — sugestão, não ordem; autonômico arbitra |
| S→R | `SAY_BEGIN / SAY_AUDIO / SAY_END / SAY_CANCEL` | turn_id, PCM/Opus 16 kHz em chunks (MEDIA) |
| R→S | `LISTEN_START / LISTEN_AUDIO / LISTEN_END` | sessão de captura pós-wake (MEDIA) |
| R→S | `EVENT` | touch{kind}, wake{score}, state_change{from,to}, fault{code}, timer_fired{id} |
| S→R | `SNAPSHOT_REQ` | modo (qvga/qqvga) |
| R→S | `SNAPSHOT_JPEG` | chunks (MEDIA), transfer_id, crc |
| S→R | `TIMER_SET / TIMER_CANCEL` | agenda local espelhada no robô |
| R→S | `STATUS` | fsm state, emotion, heap, fps, contadores de drop, métricas |
| S→R | `OTA_BEGIN / OTA_CHUNK / OTA_END` | imagem assinada; verificação antes de boot |

## 5. Autoridade e arbitragem

- O autonômico é dono da face e do corpo. `EXPRESSION_HINT`/`STIMULUS` entram
  como *inputs* do `tick()`; em conflito (ex.: touch no meio de uma fala), a
  tabela de prioridade do reflex_engine decide — nunca o server.
- `SAY_*` tem prioridade sobre expressão, mas não sobre safety nem sobre
  reflexo de toque (barge-in físico).
- Queda de sessão em fala: fade ≤ 300 ms, baseline IDLE, ícone offline.

## 6. Segurança

Token de 32 bytes provisionado junto com o WiFi (SoftAP → NVS cifrada),
comparação timing-safe nos dois lados, nunca logado em build de produção.
Sem TLS no firmware (LAN local; mbedTLS não cabe em SRAM) — mitigação: bind do
server na interface local, allowlist de IP, token obrigatório, OTA assinada
(a assinatura protege mesmo com transporte claro). Detalhes: `SECURITY.md`.

## 7. Versionamento

`proto_ver = major.minor`. Major incompatível → sessão recusada com erro
explícito. Minor → negociação de capabilities no HELLO. O YAML carrega a
versão; o codegen embute; o CI trava mudanças de wire format sem bump.
