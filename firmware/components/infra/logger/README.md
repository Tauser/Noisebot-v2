# logger

`logger` é o núcleo L1 de logs estruturados do firmware. O core é C17 puro:
sem FreeRTOS, sem `esp_*`, sem `malloc` e sem I/O síncrono.

Contrato inicial de S1.3:

- ring RAM estático de 64 registros;
- registros com `seq`, timestamp monotônico, nível, módulo e mensagem;
- mensagens sensíveis são redigidas fora de build dev;
- dump cronológico do ring para shutdown/panic via callback injetado;
- worker SD e hook de panic ficam na casca futura, depois do gate de bancada.

S1.3 só fecha de verdade quando o panic forçado em bancada produzir coredump
legível e dump do ring conforme `docs/ROADMAP.md`.
