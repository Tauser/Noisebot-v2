# grumpy_forgive

Componente `firmware/components/autonomic/grumpy_forgive` — S3.8, item 4,
camada L4 (`RFC-VIDA-V2.md` §5.2). Núcleo C17 puro do arco `GRUMPY_FORGIVE`:
"≥3 TAP em 10s -> ANGRY ~60% 1.2s -> blink lento + gaze desviado 800ms ->
NEUTRAL + micro-HAPPY 400ms. Cooldown 60s."

Compõe `nb_arc_state_t` (`arc_core`, item 3) pra fase/tempo/cooldown; a
parte nova é só a janela deslizante de TAP (anel de
`NB_GRUMPY_FORGIVE_TAP_THRESHOLD=3` timestamps) e `nb_grumpy_forgive_
current_beat()`, que traduz fase do arco + tempo decorrido dentro dela num
`nb_grumpy_forgive_beat_t` (`ANGRY`/`BLINK_GAZE_AWAY`/`NEUTRAL_HAPPY`/
`NONE`) pra casca consumir. Sem orientação visível (RFC não descreve uma
pra este arco) -- `orient_ms=0`.

Sem shell ainda -- entra quando ligado a `touch_service`/`reflex_engine`
(via `NB_REFLEX_STIMULUS_TOUCH_TAP`) e a `emotion_core`/`idle_engine` pra
de fato desenhar cada beat.
