# audio_playback_service

Componente `firmware/components/services/audio_playback_service` — S4.3,
camada L3 (`ARCHITECTURE.md`). Núcleo C17 puro
(`audio_playback_service.c/.h`): recebe `SAY_BEGIN/SAY_AUDIO/SAY_END`,
mantém um ring fixo injetado pela casca futura (PSRAM no firmware real),
entrega amostras em ordem para o writer I2S e expõe um fade curto
determinístico para `SAY_CANCEL`/queda de link sem click duro.

Sem ESP-IDF/FreeRTOS/I2S/event_bus: o componente só modela o contrato local
de playback. Host-test cobre drenagem em ordem, overflow com drop explícito,
cancelamento com fade, queda de servidor e troca de `turn_id` sem vazar
amostras do turno antigo. Desde 2026-07-09, `server_drop` com áudio já
bufferizado drena esse trecho curto e só então aplica o fade de saída,
fechando a semântica de "queda no meio da fala" usada no gate de S4.3.

Casca mínima (`shell/nb_audio_playback_service_shell.c/.h`): instância única
com ring estático e tradução explícita bytes→PCM16 do protocolo. Nesta fatia
ela já recebe `SAY_*` do `mind_link`, acumula/drena estado e contabiliza drop
de overflow, mas ainda não escreve no `audio_hal` nem cria task própria.
