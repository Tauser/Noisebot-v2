# arc_core

Componente `firmware/components/autonomic/arc_core` — S3.8, item 3, camada
L4 (`RFC-VIDA-V2.md` §5.2 "gramática geral"). Núcleo C17 puro de sequência
genérica de arco: `ORIENT -> EXECUTE -> OUTCOME -> IDLE`, com cooldown
armado só na transição natural `OUTCOME -> IDLE` (abortar via
`nb_arc_core_abort()` volta pra `IDLE` sem cooldown -- entrada em IDLE do
corpo aborta qualquer arco em qualquer fase, sem penalidade).

"Zero âncora nova" (RFC §5.2): este núcleo não conhece `emotion_core`/
`idle_engine`/`touch_service` -- só conta fases e tempo (`now_ms`
acumulado via `tick(dt_ms)`, mesmo padrão de clock injetado de
`schedule_core`/`circadian_core`). Cada arco concreto (`GRUMPY_FORGIVE`,
`RECONCILE`, `SEARCH` -- itens 4-6 do plano S3.8) usa uma instância própria
de `nb_arc_state_t` e decide, na casca ou em código específico do arco, o
que cada fase dispara nos núcleos vizinhos.

Sem shell ainda -- entra quando o primeiro arco concreto for implementado.
