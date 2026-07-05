# reflex_engine

Componente `firmware/components/autonomic/reflex_engine` — S3.2, camada L4
(`ARCHITECTURE.md`). Núcleo C17 puro (`reflex_engine.c/.h`): tabela fixa
estímulo→(prioridade, Δvalência, Δativação, evento `tiny_fsm`) dos 13
estímulos locais de `BEHAVIOR.md` §2 + `TOUCH_RELEASE`/`TOUCH_WAKE`, e
arbitragem por pilha de claims com expiração (`P0 safety > P1 touch > P2
fala > P3 hint`, `BEHAVIOR.md` §3) — camada superior suprime a inferior sem
destruí-la; ao expirar, a de baixo reaparece sozinha (reportada como
`NB_REFLEX_PRIORITY_UNCLAIMED`, a casca resolve P4 emoção/P5 idle/P6
baseline por fora, sem `reflex_engine` depender de `emotion_core`/
`idle_engine` como núcleos). Toque contínuo é reclassificado por duração
real (`WARM_PULSE` 3-8s repetindo a cada 4s, `DEEP` 8-15s, `CARESS` >15s).

`shell/nb_reflex_engine_shell.c/.h`: dá ao `event_bus` sua primeira casca
real (`NB_EVENT_TYPE_TOUCH`, publicado por `touch_service_shell` com
timestamp de `esp_timer`). Drena o bus, aplica o delta afetivo em
`emotion_core` e o evento em `tiny_fsm`; zera a pilha de claims quando
`tiny_fsm` pousa em `IDLE` (invariante X→IDLE, `ARCHITECTURE.md` §4 --
`reflex_engine` nunca disputa com ela). Chamada de dentro de
`nb_app_main_face_demo_task` (`main.c`), não roda em task própria; o pulso
sintético de estímulo do `emotion_core` (S2.5, substituto do toque real)
foi removido.

Host-test cobre: arbitragem completa (safety>touch>fala>hint), suspensão
sem destruição e retomada, empate na mesma banda (mais recente vence),
reclassificação de toque contínuo nos thresholds corretos, `force_clear`
zerando a pilha independente de expiração, estímulo desconhecido e `NULL`
seguros.

**Fora do escopo desta fatia:** medição real de latência estímulo→reação
(budget < 80ms p95, `QUALITY.md`) fica para o ensaio de bancada com toque
real -- gate de S3.2 permanece aberto até essa medição. `P2`
(fala/`SAY_*`) e `P3` (hint da mente) não têm produtor ainda (chegam em
S4); a tabela já reserva as bandas pra não precisar redesenhar a
arbitragem.
