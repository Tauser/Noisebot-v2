# led_service

Componente `firmware/components/services/led_service` — S3.3, camada L3
(`ARCHITECTURE.md`, `VISUAL.md` §6). Núcleo C17 puro (`led_service.c/.h`)
reaproveita `nb_fsm_state_t` do `tiny_fsm` (sem duplicar enum, mesmo padrão
de `emotion_core`↔`renderer`) pra escolher cor/período de respiração por
estado. Modelo two-layer herdado do v1: base (por estado, tabela fixa) +
overlay de toque (one-shot, flash quente ~150ms + fade longo ~900ms) --
`ERROR`/`SAFE_MODE` nunca suprimidos por overlay (paridade com v1). Gamma
2.2 na saída; `tick()` retorna dirty-flag (frame mudou desde a última
chamada) pra casca só escrever no RMT quando necessário -- mecanismo direto
contra flicker/tráfego redundante.

`BOOT`/`SAFE_MODE` não têm cor definida em `VISUAL.md` §6 (só
IDLE/ATTENTIVE/RESPONDING/TOUCH/SLEEPING/ERROR estão lá) -- usados branco
frio (BOOT) e laranja (SAFE_MODE) como default de engenharia, paridade
estrutural com as cores do v1 pros mesmos papéis. `RESPONDING` usa pulso
fixo (v1) até `S4.3` trazer amplitude real de fala.

"Brilho circadiano": só o mecanismo aqui
(`nb_led_service_set_brightness_scale`, `[0,1]`, multiplica antes do
gamma). A fonte real de hora-do-dia fica pro `S3.4`.

`shell/nb_led_service_shell.c/.h`: dona da instância única + `led_hal`
(RMT). Tick por frame com o estado do `tiny_fsm`; overlay de toque
disparado por `reflex_engine_shell` no mesmo lugar que já aplica em
`emotion_core`/`tiny_fsm` (reaproveita a arbitragem existente).

Host-test cobre: tabela estado→cor/onda, ERROR piscando entre cheio e piso
(nunca preto total), troca de estado reseta a fase (repetir o mesmo estado
não reseta), overlay de toque sobe/desce e desativa sozinho, overlay
ignorado em ERROR/SAFE_MODE, brilho clampado e aplicado, dirty-flag
assentando em estado estático, `NULL` seguro.
