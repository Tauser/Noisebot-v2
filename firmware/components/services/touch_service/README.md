# touch_service

Componente `firmware/components/services/touch_service` — S3.1, camada L3
(`ARCHITECTURE.md`). Núcleo C17 puro (`touch_service.c/.h`): porte
(reescrito, não copiado) do touch_service validado em produto no v1 —
mesma calibração:

- Threshold com histerese (`threshold_on`/`threshold_off`, gap de 60% do
  span de sensibilidade) pra evitar chatter perto do limite.
- EMA de sinal (α=0.20) antes da FSM operar.
- Debounce de entrada e saída (3 amostras consecutivas, ~60ms a 50Hz).
- Rejeição de proximidade: só dispara `TAP`/`WAKE` se o toque segurar
  100ms depois de confirmado (não no momento da confirmação).
- Boot stabilization (1000ms sem disparar eventos) e recalibração lenta
  de baseline (τ≈20s) só quando `IDLE` + sinal estável — proteção contra
  "baseline poisoning" durante um toque real.
- Auto-recalibração de emergência se preso em `SUSTAINED_ACTIVE` por
  >10s com sinal estável (drift de baseline, não toque real).

Sem ESP-IDF/FreeRTOS: leitura bruta e `dt_ms` são injetados via
`nb_touch_service_tick()`. Host-test cobre a FSM completa (`TAP` →
`LONG_PRESS` → `SUSTAINED`), rejeição de picos isolados de ruído,
histerese, modo `sleeping` (`WAKE` em vez de `TAP`), recalibração lenta,
proteção contra poisoning durante toque ativo, e `NULL` seguro.

Casca (`shell/nb_touch_service_shell.c/.h`): inicializa o `touch_hal`
(calibra baseline), guarda o `nb_touch_service_t` e repassa a leitura
periódica pro núcleo, com callback one-shot pros eventos — sem
event_bus ainda (mesma regra do `event_bus`/`tiny_fsm`/`idle_engine`/
`emotion_core`): só nasce quando o `reflex_engine` (S3.2) precisar
consumir de verdade. `main.c` roda uma task a 50Hz que só loga os
eventos, pro ensaio de bancada (toque intencional 50/50, zero falso
positivo em 1h de ruído ambiente).
