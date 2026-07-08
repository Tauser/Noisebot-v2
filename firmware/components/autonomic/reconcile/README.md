# reconcile

Componente `firmware/components/autonomic/reconcile` — S3.8, item 5, camada
L4 (`RFC-VIDA-V2.md` §5.2). Núcleo C17 puro do arco `RECONCILE`: "vetor em
região negativa + carinho (LONG/CARESS) -> gaze busca a frente -> suavização
1.5s -> `CONTENT` + `SLOW_BLINK` (+ ♥ se o carinho continua)". Depende da
âncora `CONTENT` (item 1).

Compõe `nb_arc_state_t` (`arc_core`, item 3): `ORIENT` = "gaze busca a
frente" (~300ms), `EXECUTE` = "suavização" (1.5s, explícito no RFC),
`OUTCOME` = "`CONTENT` + `SLOW_BLINK`" (650ms, espelha
`NB_IDLE_SLOW_BLINK_DURATION_MS` do S3.7 sem importar -- núcleos
autonômicos irmãos não se acoplam). Gatilho é uma condição de nível
(`valence<0 && carinho ativo`), não um evento discreto como o TAP de
`GRUMPY_FORGIVE` -- por isso `nb_reconcile_tick()` recebe o snapshot do
frame e decide start/abort num único ponto de entrada. Sem cooldown: o
próprio gatilho se autolimita (terminar em `CONTENT` deixa o vetor
positivo, a condição para de valer sozinha).

Sem shell ainda -- entra quando ligado a `emotion_core` (ler valência +
aplicar o empurrão pra `CONTENT`), `touch_service`/`reflex_engine`
(LONG/CARESS) e ao glifo `♥` (item 7).
