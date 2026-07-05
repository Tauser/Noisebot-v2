# circadian_core

Componente `firmware/components/autonomic/circadian_core` — S3.4, camada L4
(`ARCHITECTURE.md`, `BEHAVIOR.md` §5). Componente novo dedicado (decisão
registrada em `ROADMAP.md` S3.4) em vez de entrar dentro do `idle_engine`
(redação literal do BEHAVIOR.md), seguindo o mesmo padrão de
responsabilidade única de `reflex_engine`/`emotion_core`/`led_service`.

Núcleo C17 puro: âncora de tempo (`unix_ms` no instante atual do relógio
monotônico interno) + `tick(dt_ms)` que resolve a fase do dia (`NIGHT`
22h-6h, `DAWN` 6h-8h, `DAY` 8h-20h, `DUSK` 20h-22h; limiares `#define`
tunáveis, travados por host-test). `DAY` = brilho 1.0; `NIGHT` = piso 0.05
+ `quiet_mode`; `DAWN`/`DUSK` = rampa linear contínua entre os dois ao
longo da janela de 2h -- essa rampa (não um crossfade à parte) é o
mecanismo de "entrada/saída suave" de `SLEEPING` exigido pelo gate: o
brilho já está no piso quando `SLEEP`/`WAKE_HOUR` disparam, então a
transição de estado no `led_service` não faz "pop" visual.

Sem âncora ainda (antes do primeiro `TIME_SYNC` real ou fallback NVS): saída
neutra (`DAY`, brilho 1.0, `has_time_source=false`) -- nunca força
comportamento noturno antes de saber a hora de verdade.

Sem FreeRTOS/ESP-IDF/NVS/protocolo: âncora e `dt_ms` injetados. Host-test
cobre: limiares de fase por hora, brilho pleno em `DAY`/piso em `NIGHT`,
rampa monotônica em `DAWN`/`DUSK`, `quiet_mode` só em `NIGHT`, default
neutro sem âncora, recalibração por `set_time_anchor` (não acumula com a
âncora anterior), avanço de `now_unix_ms` com o monotônico, `NULL` seguro.

**Fora do escopo desta fatia:** sem servidor real enviando `TIME_SYNC`
ainda (protocolo já tem codegen -- `protocol/generated/c/nbp2.c` -- mas
nada em `server/` chama `encode_time_sync`); o gate foi validado com
relógio injetado acelerado em bancada (ver `shell/`), mesmo padrão do
pulso sintético do `emotion_core` antes do toque real (S2.5→S3.1).
Compensação de latência de rede via `server_mono_ms` do `TIME_SYNC`
(RTT) fica pra quando isso importar de verdade -- por ora a âncora usa só
`unix_ms` diretamente.
