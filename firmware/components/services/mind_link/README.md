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

**Avanço S4.3 (2026-07-08, parcial):** a mesma casca agora também decodifica
o downlink `SAY_BEGIN/SAY_AUDIO/SAY_END/SAY_CANCEL`. Por enquanto o áudio
recebido ainda não disputa o speaker local de bring-up; ele entra como
telemetria/hint de turno (`NB_EVENT_TYPE_MIND_HINT`) para o corpo reagir ao
início/fim/cancelamento da fala e limpar `RESPONDING` se o link cair no meio
da resposta.

**Avanço S4.3 (2026-07-09, parcial):** `server/tests/test_nbp2_fake_server.py`
agora valida no host o fluxo mínimo contra `tools/nbp2_fake_server.py`:
`HELLO`/`HELLO_ACK`, uplink `LISTEN_START/LISTEN_AUDIO/LISTEN_END` e downlink
`SAY_BEGIN/SAY_AUDIO/{SAY_END|SAY_CANCEL|drop}`. Isso não substitui a bancada,
mas cria um gate executável para o wire protocol da fatia sem depender da
Waveshare nem da mente real.

**Avanço S4.3 (2026-07-09, parcial):** o downlink `SAY_*` agora também alimenta
`audio_playback_service_shell`: `SAY_BEGIN` abre um turno local de playback,
`SAY_AUDIO` entra em ring fixo com contagem explícita de drop por overflow, e
`SAY_END`/`SAY_CANCEL`/queda de link fecham esse turno no serviço local. O
speaker real ainda não toca nesta fatia, mas o `mind_link` deixou de ser só
telemetria de turno e passou a abastecer o pipeline local que a próxima casca
vai drenar para o `audio_hal`.

**Fechamento S4.3 (2026-07-09):** a bancada em `COM5` contra o fake server
validou `HELLO_ACK`/`READY`, sessão completa `LISTEN_*`, downlink
`SAY_BEGIN/SAY_AUDIO` e reconexão após queda induzida do link no meio da
fala. O fade local do drop ficou coberto pelo host-test do
`audio_playback_service`, que agora drena o trecho já bufferizado antes do
fade de 300 ms.

**Ainda pendente para fases futuras:** substituir o writer temporário de
`audio_bringup` por `audio_service` definitivo, expor métricas de queue/drop
em `STATUS` e fazer soak prolongado contra a mente real.
