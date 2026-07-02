# NoiseBot 2 — Documento de Projeto

## Visão

NoiseBot 2 é um companion robot desktop expressivo que parece vivo: responde ao
toque, olha para você, se move com intenção e mantém uma persona recognoscível
mesmo em repouso. Acoplado a um server local privado, ele conversa, lembra,
reconhece quem está na frente dele e executa tarefas — sem nuvem, sem conta,
sem assinatura.

É a segunda geração do NoiseBot. A primeira provou o produto (face paramétrica,
voz fim-a-fim, visão, persona) e pagou caro por duas decisões: cognição em C
embarcado e arquitetura dual-MCU. O v2 nasce das lições registradas na análise
arquitetural de 2026-07-01 do repositório original.

## Tese arquitetural

**Corpo e mente.** Um único ESP32-S3 é o corpo e o sistema nervoso autonômico:
tudo que faz o robô parecer vivo sozinho numa mesa. O server Python local é a
mente: tudo que exige linguagem, memória ou raciocínio. Esta divisão é a mesma
dos produtos de referência do segmento (StackChan comercial/CoreS3 + XiaoZhi,
EMO + cloud, Vector + cloud) — com a diferença de que a nossa mente é local e
privada.

**Piso offline (contrato de produto):** sem server, o NoiseBot 2 mantém idle
vivo, expressões, reação a toque, ciclo circadiano, sono, timers/alarmes e
safety. Ele fica *quieto*, nunca *morto*. A degradação é visível (ícone) e
suave (termina o gesto, volta ao baseline).

## Princípios de engenharia

| # | Princípio | Consequência prática |
| --- | --- | --- |
| P1 | Corpo e mente, não cérebro duplo | Um MCU, um firmware, um protocolo. Regra da mesa decide onde cada comportamento mora |
| P2 | Enforcement antes de feature | CI no commit 1; invariantes testadas quando nascem; budget é número no CI, não prosa |
| P3 | Núcleo funcional, casca imperativa | Toda lógica é lib C17 pura testável no host; FreeRTOS/ESP-IDF só na casca |
| P4 | Contrato é artefato | `protocol/nbp2.yaml` → codegen C + Python + golden tests de bytes. Zero espelhamento manual |
| P5 | Complexidade só depois de medir | Nenhum gasto estrutural sem medição registrada. Foi assim que o v1 comprou um enlace inter-MCU desnecessário |

## Princípios de comportamento (herdados do v1 — continuam válidos)

| Princípio | Descrição |
| --- | --- |
| Presença em repouso | Idle não é estático; é movimento mínimo vivo |
| Gaze como atenção | Direção do olhar precede qualquer expressão |
| Neutral forte | A face de repouso é desenhada com intenção |
| Movimento econômico | Início claro, destino claro, retorno limpo ao neutral |
| Touch é vínculo | Resposta < 80 ms percebida, calorosa, contextual |
| Timing social | Pausas, pre-speech, post-speech settle são produto |
| Coordenação de output | Face, motion e áudio orquestrados; nunca independentes |
| Estado interno modula | Emotion vector (valência × ativação) influi sutilmente em tudo |
| Identidade local | Perfil offline-first do usuário; biometria é conveniência, não requisito |

## Hardware

Um único **Freenove ESP32-S3-WROOM CAM N16R8** com todos os periféricos do v1:
ST7789 2", OV2640, microSD, INMP441 + MAX98357A (I2S full-duplex), 2× SCS0009
(barramento 1 fio), 2× WS2812, touch capacitivo de cobre. USB nativo
restaurado. Mapa completo e budgets: `HARDWARE.md`.

A mente roda no PC do usuário (GPU): faster-whisper, Ollama/OpenAI, Piper,
OpenCV — herdados do server v1 por refactor.

Waveshare N32R16: fora do produto (spare; possível rig de HIL futuro, nunca
dependência).

Adiados (entram por I2C sem mudança de pinout): IMU MPU-6050, bateria/fuel
gauge, touch 3 zonas (MPR121), touchscreen.

## O que o v2 explicitamente não faz

- Não roda LLM/STT/TTS no firmware.
- Não usa nuvem para nenhuma função essencial (busca web é opcional e
  degradável).
- Não move servo sem safety verde + gate elétrico assinado.
- Não mantém dois donos de qualquer verdade (visual, temporal, de estado).

## Definição de sucesso

Paridade funcional com o v1 (voz, face, touch, visão, persona, agenda) com:
um firmware de ~1/3 do tamanho, cobertura de host-test alta nos núcleos,
CI honesto, OTA assinada, soak de 7 dias sem crash — e o mesmo carisma.
