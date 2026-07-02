# NoiseBot 2

Companion robot desktop expressivo, **offline-first**, com agente local privado.
Segunda geração do NoiseBot: um único ESP32-S3 como corpo + sistema nervoso
autonômico, e um server Python local (PC com GPU) como mente.

**Arquitetura em uma frase:** o robô é um pet completo sem rede; com o server
na LAN ele ganha voz, memória e visão semântica. A cognição nunca mora no
firmware; a vida nunca depende do server.

## Topologia

```
Waveshare ESP32-S3 N32R16V (robô)   ◄── NBP/2 sobre TCP/LAN ──►  Server Python (mente)
face ST7789 · áudio I2S duplex · wake                            STT · LLM · TTS
touch · servos SCS (UART) + safety · LEDs                        persona · memória SQLite
microSD · autonômico (câmera adiada)                             tools · dashboard React
```

## Documentos

| Doc | Conteúdo |
| --- | --- |
| [`docs/PROJECT.md`](docs/PROJECT.md) | Visão, tese, princípios P1–P5 |
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | Camadas, fronteira corpo/mente, padrões obrigatórios |
| [`docs/HARDWARE.md`](docs/HARDWARE.md) | Mapa de pinos single-MCU (Waveshare, ativa), budgets de memória |
| [`docs/HARDWARE_FREENOVE.md`](docs/HARDWARE_FREENOVE.md) | Rota alternativa Freenove (não ativa) + procedimento de troca |
| [`docs/BEHAVIOR.md`](docs/BEHAVIOR.md) | FSM, modelo emocional, reflexos, arbitragem |
| [`docs/VISUAL.md`](docs/VISUAL.md) | Face paramétrica, 10 expressões, motifs de idle, overlays, LEDs |
| [`docs/MEMORY.md`](docs/MEMORY.md) | Memória de curto/médio/longo prazo, privacidade, wipe |
| [`docs/PROTOCOL.md`](docs/PROTOCOL.md) | NBP/2 — framing, semântica, schema-first |
| [`docs/SERVER.md`](docs/SERVER.md) | A mente: atores, turno de voz, providers, tools, dashboard |
| [`docs/VOICE.md`](docs/VOICE.md) | Pipeline de voz do corpo: wake, sessão, invariantes V-1..V-6, playback |
| [`docs/ENERGY.md`](docs/ENERGY.md) | Orçamento de energia, trilhos, brownout, gate elétrico S6.1 |
| [`docs/ROADMAP.md`](docs/ROADMAP.md) | Fases S0–S7 com subfases, gates e status — **fonte de verdade da execução** |
| [`docs/QUALITY.md`](docs/QUALITY.md) | CI, definition of done, budgets mensuráveis |
| [`docs/SECURITY.md`](docs/SECURITY.md) | Secure Boot, OTA assinada, tokens, provisioning |
| [`docs/S0_BRINGUP.md`](docs/S0_BRINGUP.md) | Procedimento de bancada dos spikes S0 |

## Estrutura

```
firmware/    ESP-IDF, C17 (C++ só no renderer) — corpo e autonômico
protocol/    nbp2.yaml (schema único) + codegen C/Python
server/      Python 3.10+ — a mente (atores sobre event bus)
tools/       scan de segredos, gate de SRAM, utilitários
docs/        documentação fundadora
.github/     CI — build -Werror, host-tests, golden de protocolo, pytest
```

## Regras que não se negociam

1. ESP-IDF puro (nunca Arduino), C17, `-Wall -Wextra -Werror`.
2. Nenhum servo se move antes de `motion_safety` verde + gate elétrico assinado.
3. Todo núcleo de lógica é C17 puro testável no host (clock/I/O injetados).
4. Protocolo nasce do schema (`protocol/nbp2.yaml`); nunca editar encoder/decoder à mão.
5. CI verde é pré-condição de merge. Sem filtro de paths em testes cruzados.
6. Nenhum segredo em código, log de produção ou git.

Histórico e racional das decisões: ver análise arquitetural do v1
(2026-07-01) no repositório `noisebot` original.

## Licença

[MIT](LICENSE).
