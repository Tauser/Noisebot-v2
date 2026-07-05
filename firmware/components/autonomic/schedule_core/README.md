# schedule_core

Componente `firmware/components/autonomic/schedule_core` — S3.5, camada L4
(`ARCHITECTURE.md`, `BEHAVIOR.md` §5). Núcleo C17 puro: array fixo de
`NB_SCHEDULE_MAX_TIMERS=8` slots, disparo one-shot em `fire_at_unix_ms`
absoluto (sem recorrência -- o protocolo NBP/2 `TIMER_SET`/`TIMER_CANCEL`/
`EVENT_TIMER_FIRED`, já com codegen, não define campo de recorrência).
`create()` faz upsert por `timer_id` (retry idempotente do protocolo não
duplica); `tick(now_unix_ms)` dispara os slots vencidos e libera cada um
no mesmo tick -- nunca duas vezes. Layout sem ponteiros, compatível com
blob de NVS.

`shell/nb_schedule_core_shell.c/.h`: persiste o array inteiro em NVS
(namespace próprio `"nb_sched"`, blob único, mesmo padrão de
`mind_link_token_shell` -- o modelo escalar por chave do `app_config` não
serve pra um array de slots) a cada mutação. `tick(dt_ms)` usa
`nb_circadian_core_shell_now_unix_ms()` como fonte de hora. Cobre "reboot
não perde nem duplica": ao reiniciar, carrega o blob salvo; timer vencido
enquanto desligado dispara no primeiro tick (atrasado, mas dispara) e sai
liberado do próprio disparo.

`NB_EVENT_TYPE_TIMER` (novo em `event_bus`) carrega `TIMER_SET`/
`TIMER_CANCEL` de `mind_link_shell` (L3) pro `schedule_core` (L4) --
"camada chama só pra baixo" proíbe L3 chamando L4 direto, então vai pelo
bus; `reflex_engine_shell` (único leitor do bus) despacha, mesmo padrão de
`TIME_SYNC`→`circadian_core` (S3.4). Rótulo de `TIMER_SET` remoto não cabe
nos 16 bytes do payload do bus (fica vazio) -- rótulo completo só importa
quando houver criação local por voz (S4.6).

Disparo de timer aplica `NB_REFLEX_STIMULUS_TIMER_FIRED` (banda P3/HINT,
reaproveitada em vez de abrir uma 5ª banda de claim) via
`nb_reflex_engine_shell_apply_stimulus()`, e `mind_link_shell_send_
timer_fired()` (gated em `NB_MIND_LINK_STATE_READY`, fire-and-forget --
sem server, o reflexo local já disparou de qualquer jeito). "Som local" de
BEHAVIOR.md §5 fica pra quando `audio_hal` existir (S4.1).

Host-test cobre: criar/obter, rótulo `NULL`/truncado, upsert por id
repetido (não duplica), array cheio + cancelar libera espaço, cancelar
inexistente, disparo limpa o slot e não dispara duas vezes, respeito ao
limite de disparos por tick (retry na próxima chamada), timer ainda não
vencido fica pendente, `NULL` seguro.
