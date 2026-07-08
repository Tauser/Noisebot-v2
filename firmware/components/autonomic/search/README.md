# search

Componente `firmware/components/autonomic/search` — S3.8, item 6, camada
L4 (`RFC-VIDA-V2.md` §5.2). Núcleo C17 puro do arco `SEARCH`: "orientação
(~300ms) -> procura (2-4 vistadas sacádicas, padrões `LATERAL`/`DIAGONAL`/
`ORBIT`) -> desfecho obrigatório (achou: perk+tilt+micro-HAPPY; não achou:
blink lento + `SIGH`)".

Só decide **qual** padrão/quantas vistadas/se achou (RNG próprio,
xorshift32) -- calcular os ângulos de gaze de cada padrão é extensão do
motor de atenção (`idle_engine`/`nb_attention`, S3.7), fora de escopo
aqui ("extensão do motor de atenção, não mecanismo novo", plano S3.8).
Padrão nunca repete o anterior; NB2 não tem câmera, então achar/não achar
é floreio comportamental (RNG), não detecção real.

Dois gatilhos com regras diferentes: `nb_search_trigger_stimulus()` sem
limite de taxa próprio; `nb_search_trigger_boredom()` limitado a
`NB_SEARCH_BOREDOM_MIN_INTERVAL_MS` (1h, RFC "tédio longo em companion
<=1/h") -- rastreado neste núcleo, não pelo `cooldown_ms` de `arc_core`
(que fica 0, senão bloquearia também o gatilho de estímulo).

Sem shell ainda -- entra quando ligado a `reflex_engine` (estímulo) e ao
contador de tédio de `idle_engine`/motor de energia (S3.7).
