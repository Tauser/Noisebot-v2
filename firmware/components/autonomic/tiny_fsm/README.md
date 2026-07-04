# tiny_fsm

Componente `firmware/components/autonomic/tiny_fsm` — S2.3, camada L4
(`ARCHITECTURE.md`). Núcleo C17 puro (`tiny_fsm.c/.h`): os 8 estados de
`BEHAVIOR.md` §1 (`BOOT`, `IDLE`, `ATTENTIVE`, `RESPONDING`,
`TOUCH_REACTING`, `SLEEPING`, `ERROR`, `SAFE_MODE`), os 3 modos como flags
persistentes (`quiet`, `companion`, `maintenance` — sobrevivem a troca de
estado, só mudam por evento explícito de entrada/saída) e as regras de
transição de §1.2 (safety vence tudo, touch vence fala, wake ignorado em
`SLEEPING` + modo `quiet`, `SAFE_MODE` pegajoso).

Sem FreeRTOS/ESP-IDF/event_bus — eventos chegam por chamada direta de
`nb_tiny_fsm_apply_event()`. Casca ainda não nasceu (mesma regra do
`event_bus` em S1.2): só quando um consumidor real (ex.: `reflex_engine`,
S3.2) precisar traduzir eventos do event bus pra esta API.

**Invariante central (gate H7, `ARCHITECTURE.md` §4):** o núcleo mantém
`nb_fsm_transient_t` — o estado transitório que cada estado não-IDLE liga
(motif de atenção, boca ativa, reação de toque, olhos fechados, ícones de
erro, gaze). Toda transição para `IDLE` zera esse struct por completo,
não importa de qual estado veio nem quais modos estavam ativos; os modos
em si **não** são zerados (persistem até evento explícito de saída).
Host-test cobre 100% das arestas documentadas, as combinações inválidas
(no-op), `FAULT`/`FAULT_CRITICAL` de qualquer estado, `SAFE_MODE`
pegajoso, e o invariante X→IDLE cruzado com todas as combinações de modo.
