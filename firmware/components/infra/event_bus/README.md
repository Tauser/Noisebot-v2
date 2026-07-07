# event_bus

`event_bus` é o primeiro componente L1 de infraestrutura do NoiseBot 2. O
núcleo é C17 puro: sem FreeRTOS, sem `esp_*`, sem `malloc` e sem acesso a
I/O real.

Contrato inicial de S1.2:

- pool estático com 32 slots, sendo 4 reservados para safety;
- publicações normais nunca consomem slot reservado de safety;
- eventos de safety usam fila própria e são entregues antes dos eventos
  normais;
- ring de auditoria guarda publish/drop/poll com contadores de pendência;
- a casca FreeRTOS entra apenas quando houver serviços publicando eventos.

O núcleo não assume concorrência interna. A casca serializa `publish/poll`
com critical section (`portENTER_CRITICAL`/`portEXIT_CRITICAL`).

**S3.2:** primeira casca real (`shell/nb_event_bus_shell.c`, instância única
protegida por critical section) — `touch_service_shell` publica
`NB_EVENT_TYPE_TOUCH`, `reflex_engine_shell` faz o poll.

**S4.2:** `wake_service_shell` passou a publicar `NB_EVENT_TYPE_VOICE`.
`nb_voice_event_payload_t.kind` distingue wake/listen/feedback e
`detail` carrega metadado coarse específico por evento (nível de áudio,
motivo de end, feedback honesto) sem estourar os 16 bytes do payload.
