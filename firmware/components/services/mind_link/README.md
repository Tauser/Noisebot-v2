# mind_link

Núcleo C17 puro da sessão do mind_link (S1.7 — `PROTOCOL.md` §3), primeiro
componente em `firmware/components/services/` (camada L3 de
`ARCHITECTURE.md`). Sem FreeRTOS, sem sockets, sem ESP-IDF; timestamps
injetados pelo chamador.

Contrato do núcleo:

- máquina de estados da sessão: `IDLE` → `AWAITING_HELLO_ACK` (transporte
  conectou, HELLO enviado) → `READY` (HELLO_ACK bateu o `boot_id`) →
  `DEAD` (timeout de heartbeat, erro de transporte, ou `boot_id` divergente
  no HELLO_ACK — PROTOCOL.md §3.4: "boot_id novo de qualquer lado invalida
  estado de sessão pendente");
- heartbeat: `tick()` sinaliza quando mandar heartbeat (a cada
  `NB_MIND_LINK_HEARTBEAT_INTERVAL_MS` = 1 s) e mata a sessão depois de
  `NB_MIND_LINK_MAX_MISSED_HEARTBEATS` (3) intervalos sem heartbeat do
  server — literal do PROTOCOL.md §3.3 ("Heartbeat 1 s em ambas direções; 3
  perdidos → sessão morta, robô continua autônomo e reconecta");
- backoff de reconexão exponencial determinístico (sem jitter, decisão
  consciente para manter testável): `min(500 ms * 2^tentativa, 30 s)`;
  tentativas zeram no primeiro `HELLO_ACK` bem-sucedido.

**Avanço S4.3 (2026-07-07, parcial):** além de `HEARTBEAT` e
`EVENT_TIMER_FIRED`, a casca passou a aceitar pedidos externos enfileirados
para `EVENT_WAKE` e `LISTEN_START/LISTEN_AUDIO/LISTEN_END` por uma fila
estática protegida por critical section. O socket continua sendo propriedade
exclusiva da task `mind_link`; produtores externos só enfileiram intent de
envio, nunca fazem `send()` direto.

**Ainda pendente:** recepção de `SAY_*`, backpressure/queda no meio do
stream, métricas de queue/drop para `STATUS`, e soak completo do fluxo contra
um server fake ou a mente real.
