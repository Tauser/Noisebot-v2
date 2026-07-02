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

O núcleo não assume concorrência interna. A casca futura deve serializar
`publish/poll` com critical section ou task dona do bus.
