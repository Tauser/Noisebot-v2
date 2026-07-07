# wake_service

Componente `firmware/components/services/wake_service` — S4.2, camada L3
(`ARCHITECTURE.md`, `VOICE.md` §2-4). Núcleo C17 puro (`wake_service.c/.h`)
da sessão de wake/VAD: só wake word arma escuta, VAD fora de sessão não
publica atividade, `LISTEN_START` só nasce depois de wake, `LISTEN_END`
nunca sai órfão, e indisponibilidade de VAD/rota útil (mente ou intent
local) vira feedback honesto em vez de fallback silencioso.

Esta fatia é deliberadamente só o núcleo de arbitragem de sessão, sem
ESP-SR, sem event_bus e sem I2S: a casca futura injeta wake score, frames
classificados pelo VAD real e conectividade da mente. A primeira amostra de
fala vira a ação `LISTEN_BEGIN_WITH_AUDIO`, para a casca publicar
`LISTEN_START` + primeiro `LISTEN_AUDIO` no mesmo instante, sem quebrar o
invariante V-4.

`shell/nb_wake_service_shell.c/.h` já existe nesta fase como casca mínima
de integração: instância única, API explícita pros futuros produtores
(WakeNet/ESP-SR/harness) e publicação de `NB_EVENT_TYPE_VOICE` no
`event_bus`. Ainda não cria task própria nem lê hardware direto.

Desde 2026-07-07, a casca também registra a latência local de
`WAKE`→`LISTEN_START` no próprio log (budget de 250 ms de `VOICE.md`) e
publica um nível coarse de intensidade (`SOFT`/`LOUD`) nos eventos
`LISTEN_AUDIO`, derivado do produtor chamador. Isso não abre sessão nem faz
VAD heurístico fora de contexto; serve só para reflexos/telemetria dentro
de uma sessão já válida.

Na mesma data, a casca ganhou um `voice_sink` explícito: `main.c` pode
receber `SESSION_ARMED`/`LISTEN_*` com o PCM cru do chunk aceito e
encaminhar isso ao `mind_link_shell` sem acoplar `wake_service` diretamente
ao transporte. Isso prepara o streaming NBP/2 de S4.3 preservando o
contrato do núcleo puro (`wake_service.c`) e a regra "task do socket é dona
exclusiva do send()".

`main/Kconfig.projbuild` agora também expõe um **harness explícito de
bancada** (`CONFIG_NB_WAKE_BENCH_HARNESS`), desligado por padrão: a task
temporária `audio_bringup` pode emular wake/VAD por RMS do microfone para
exercitar sessão/publicação antes da integração com WakeNet/ESP-SR. Isso é
somente ferramenta de bancada para S4.2, nunca fallback silencioso de
produção (VOICE.md V-5).

Para repetir essa bancada sem editar `sdkconfig`, use o perfil
`firmware/sdkconfig.s4_2_wake_bench.defaults`:

```powershell
idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.s4_2_wake_bench.defaults" build
```

Ele só liga o harness e seus thresholds padrão (`wake=1800`, `vad=700`,
`cooldown=3000 ms`), preservando `sdkconfig.defaults` como baseline.

Host-test cobre os invariantes V-1..V-6 que já são verificáveis sem
hardware: toque nunca abre voz; wake sem VAD falha honestamente; wake sem
rota falha honestamente; áudio fora de sessão não publica nada; primeira
fala após wake abre `LISTEN_START`; silêncio e duração máxima encerram só
depois de áudio real; perda de rota no meio da sessão não pendura.
