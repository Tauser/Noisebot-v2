# NoiseBot 2 — Roadmap Canônico

**Status:** ativo
**Autoridade:** este documento define ordem, IDs, dependências e gates da
construção do NoiseBot 2. `CLAUDE.md` define regras de código;
`ARCHITECTURE.md` define a arquitetura-alvo. Em divergência de status,
**este arquivo prevalece**.

## 1. Como ler

| Status | Significado |
| --- | --- |
| `PENDENTE` | Ainda não iniciada |
| `EM ANDAMENTO` | Trabalho ativo com ID declarado |
| `FEITO` | Todos os critérios de saída atendidos, com evidência registrada |
| `BLOQUEADO` | Aguardando dependência explícita |
| `FALLBACK` | Concluída pela alternativa documentada (registrar qual) |

Regras de leitura de evidência (herdadas do v1, onde funcionaram):

- Build limpo prova compilação; não prova hardware, latência nem produto.
- Log de um só lado não prova fluxo ponta a ponta.
- ACK de frame prova recepção, não aplicação de domínio.
- Spike não autoriza fase seguinte.

## 2. Governança

1. Só existem os IDs `S0.*`–`S7.*` registrados na seção 4. É **proibido**
   criar fases, subfases ou sufixos não registrados.
2. Se um trabalho necessário não couber em um ID existente, parar e propor
   alteração deste documento declarando motivo, dependências, gate e impacto
   na ordem.
3. Subfase vira `FEITO` somente com todos os critérios de saída atendidos e
   evidência registrada (log, medição, tag ou doc em `docs/bringup/` /
   `docs/releases/`).
4. Commits pequenos, um ID por commit (`S2.3: ...`).
5. Ao iniciar trabalho, declarar o ID. Ao terminar, listar gates executados e
   pendentes.
6. O NoiseBot v1 continua operacional durante toda a construção. Nada do v1 é
   desligado antes de S7.5.
7. Dependências de hardware não cabeado nunca são trabalho principal do ciclo.

## 3. Painel

| Campo | Decisão |
| --- | --- |
| Fase atual | S0 — spikes de bancada |
| Próximo marco | Pinout congelado (S0.4, tag `pinout-v1.0`) |
| Hardware | Freenove N16R8 única; Waveshare fora do produto (spare) |
| Servo | 1 fio GPIO3 (S0.1); fallback documentado: 19/20 como no v1 |
| Maior risco atual | S0.3 (contenção câmera+render+áudio) nunca foi medido em nenhuma das gerações |
| Regra de ouro | CI verde é pré-condição de merge desde S1.1 |

## 4. Fases e subfases

### S0 — Spikes de bancada e congelamento de pinout

*Objetivo:* provar as três hipóteses de hardware que o design assume.
*Dependências:* nenhuma. *Procedimentos completos:* `S0_BRINGUP.md`.
*Regra:* código de spike vive fora da árvore de produto (`scratch/spikes/`) e
morre depois; nenhuma linha dele é promovida sem reescrita pelo padrão P3.

| ID | Entrega | Gate de saída | Status |
| --- | --- | --- | --- |
| S0.1 | Servo SCS 1 fio no GPIO3 (UART half-duplex via matrix, open-drain) | PING ≥ 999/1000; leitura 20 Hz/10 min sem erro; bordas limpas no osciloscópio; boot normal com servo conectado | `PENDENTE` |
| S0.2 | WS2812 externos no GPIO46 com pixel sacrificial | 30 min sem flicker; 20/20 reboots; 20/20 uploads esptool; LED onboard independente | `PENDENTE` |
| S0.3 | Contenção câmera+display+áudio simultâneos | zero underrun em 30 min; fps ≥ 28 durante captura; JPEG < 300 ms p95; PSRAM livre ≥ 4 MB | `PENDENTE` |
| S0.4 | Pinout congelado | `HARDWARE.md` sem marcador SPIKE; evidências em `docs/bringup/`; tag `pinout-v1.0` | `PENDENTE` |

### S1 — Fundação (infra + segurança + CI)

*Objetivo:* esqueleto executável com toda a infraestrutura de qualidade e
segurança — antes de qualquer feature de produto.
*Dependências:* S0.4 (apenas para S1.6+; S1.1–S1.5 podem começar em paralelo
ao S0). *Camadas:* L0 parcial, L1, início do mind_link.

| ID | Entrega | Gate de saída | Status |
| --- | --- | --- | --- |
| S1.1 | Repo + CI completo (`QUALITY.md` §1): build `-Werror`, host-tests, lint, secrets-scan, budget-gates (tetos iniciais) | CI verde no primeiro `main.c`; PR de teste com warning proposital fica vermelho | `PENDENTE` |
| S1.2 | `event_bus` (pool estático, slots de safety, fila de safety, ring de auditoria) **com teste de burst no mesmo commit** | host-test: zero drop não-safety sob perfil de burst alvo; safety imune a fila cheia | `PENDENTE` |
| S1.3 | `logger` estruturado (ring RAM + worker SD) + dump de ring em shutdown **e panic** (coredump partition) | panic forçado em bancada produz coredump legível + ring de eventos | `PENDENTE` |
| S1.4 | `config` (NVS tipada, chaves centralizadas) + `boot_manager` por fases com relatório | boot < 3 s até task idle; falha de fase crítica → SAFE_MODE testado | `PENDENTE` |
| S1.5 | `watchdog` (TWDT + HW) integrado a todas as tasks existentes | task travada em bancada → reset + causa registrada em NVS | `PENDENTE` |
| S1.6 | WiFi + **provisioning SoftAP** (SSID/senha/token → NVS) | provisionar do zero pelo celular sem toolchain; `secrets-scan` confirma zero credencial no repo | `PENDENTE` |
| S1.7 | NBP/2 núcleo: codegen do `nbp2.yaml` (C+Python), framing/CRC32, HELLO+token timing-safe, HEARTBEAT, TIME_SYNC, EVENT, STATUS, reconexão com backoff | golden tests C↔Python no CI; HELLO sem/erro de token → conexão encerrada (teste dos dois lados); soak de reconexão 100 ciclos | `PENDENTE` |
| S1.8 | OTA A/B assinada + anti-rollback + Secure Boot v2 + flash encryption (chaves geridas por `SECURITY.md` §3) | OTA ida-e-volta em bancada; imagem adulterada recusada; dump de flash não revela token; procedimento de recuperação de chave documentado | `PENDENTE` |
| S1.9 | Soak do esqueleto | 24 h: zero reset, heap estável, reconexões limpas com server de teste | `PENDENTE` |

### S2 — Face (o robô fica vivo, mudo)

*Objetivo:* display + renderer + FSM + idle. No fim de S2 o robô parece vivo.
*Dependências:* S1.9. *Referência de implementação:* renderer do head v1 (DM2).

| ID | Entrega | Gate de saída | Status |
| --- | --- | --- | --- |
| S2.1 | `display_hal` (ST7789 SPI 50 MHz, 3 pinos, double buffer PSRAM, wrapper `extern "C"`) | padrão de teste a 30 fps por 1 h; zero artefato; SRAM inalterada (gate do `.map`) | `PENDENTE` |
| S2.2 | Renderer paramétrico (9 expressões, interpolação 220 ms, AA sub-pixel) | paridade visual com v1 confirmada lado a lado; fps ≥ 30 medido | `PENDENTE` |
| S2.3 | `tiny_fsm` (8 estados) **nascendo com o teste de invariante X→IDLE** | host-test cobre 100% das transições; invariante verde | `PENDENTE` |
| S2.4 | `idle_engine` (blink Poisson, gaze errante/saccades, micro-behaviors, respiração) | 10 min de observação: nunca estático, nunca repetitivo óbvio; parâmetros documentados | `PENDENTE` |
| S2.5 | `emotion_core` v0 (vetor+decaimento) modulando neutral/idle sutilmente | host-test de decaimento e integração de estímulo; efeito visível em bancada | `PENDENTE` |
| S2.6 | Gate visual da fase | soak 48 h face viva sem crash; budgets de fps/PSRAM registrados como baseline | `PENDENTE` |

### S3 — Toque, LEDs e reflexos (pet completo offline)

*Objetivo:* fechar o piso offline do produto.
*Dependências:* S2.6 (S3.1 pode começar após S2.3).

| ID | Entrega | Gate de saída | Status |
| --- | --- | --- | --- |
| S3.1 | `touch_hal` + `touch_service` (calibração do v1 2.2A: threshold 20%, debounce, TAP/LONG/SUSTAINED) | toque intencional 50/50; zero falso positivo em 1 h de ruído ambiente; reproduzível após reboot | `PENDENTE` |
| S3.2 | `reflex_engine` (tabela estímulo→reação com prioridades; touch→afeto integra emotion+face) | host-test da tabela de arbitragem (conflitos touch×idle×sleep); reação < 80 ms p95 medida | `PENDENTE` |
| S3.3 | `led_service` (WS2812 no 46; idle/estados/afeto; brilho circadiano) | paridade com linguagem de LED do v1; sem flicker | `PENDENTE` |
| S3.4 | Ciclo circadiano + sono (SLEEPING com entrada/saída suaves) | transições dormir/acordar observadas nos horários; invariante IDLE segue verde | `PENDENTE` |
| S3.5 | `schedule_core` (timers/alarmes/lembretes locais, persistência NVS, disparo→reflexo+face+led) | criar/cancelar/disparar OK; reboot não perde nem duplica; disparo com server offline funciona | `PENDENTE` |
| S3.6 | Gate do piso offline | soak 48 h em modo pet (sem server): vivo, responsivo, estável | `PENDENTE` |

### S4 — Voz (o robô conversa)

*Objetivo:* pipeline de voz fim-a-fim com a mente.
*Dependências:* S1.7, S3.6. *Referência:* Voice Audio v2 do v1 (desenho) e
server v1 (refactor).

| ID | Entrega | Gate de saída | Status |
| --- | --- | --- | --- |
| S4.1 | `audio_hal` I2S full-duplex 16 kHz (mic+spk no mesmo barramento) | loopback limpo; zero underrun em 30 min com render ativo (re-valida S0.3 na árvore real) | `PENDENTE` |
| S4.2 | `wake_service` (WakeNet) + VAD (ESP-SR) com política de sessão do v1 (VAD só decide dentro de sessão aberta por wake) | wake em ambiente real ≥ 9/10; falso-wake < 1/h; overlay listening < 250 ms | `PENDENTE` |
| S4.3 | Streaming NBP/2 de áudio (LISTEN_* robô→server; SAY_* server→robô; canal MEDIA com backpressure; barge-in físico por touch) | golden tests; sessão completa contra server fake; queda de link no meio da fala → fade ≤ 300 ms + IDLE | `PENDENTE` |
| S4.4 | Server: `TurnEngine` + `MindOutput` extraídos do orchestrator v1 (atores sobre bus, nenhum ator chama outro) | testes de turno portados do v1 passam na nova estrutura; barge-in cancela task de turno | `PENDENTE` |
| S4.5 | Providers ligados: faster-whisper, Ollama/OpenAI com circuit breaker, Piper | conversa fim-a-fim em PT-BR; falha de LLM degrada com resposta honesta, sem travar FSM | `PENDENTE` |
| S4.6 | Intents locais offline-first (hora, timer, status) respondendo sem LLM | intents respondem com LLM desligada; latência < 1 s | `PENDENTE` |
| S4.7 | Gate de voz | budgets §4 de `QUALITY.md` medidos e registrados (wake→listening, fala→primeiro áudio); soak 24 h com conversas periódicas | `PENDENTE` |

### S5 — Visão (presença e identidade)

*Objetivo:* câmera sob demanda + pipeline semântico na mente.
*Dependências:* S4.7. *Referência:* DM4/13.1 do v1.

| ID | Entrega | Gate de saída | Status |
| --- | --- | --- | --- |
| S5.1 | `camera_hal` (`esp_camera`, OV2640, QVGA JPEG sob demanda — lição DM4.8: não usar esp_video) | captura estável; zero interferência em render/áudio (contadores) | `PENDENTE` |
| S5.2 | `SNAPSHOT_*` no NBP/2 (chunks MEDIA, transfer_id, CRC, preempção por CTRL) | JPEG íntegro no server < 400 ms p95; controle nunca atrasado por transferência (medido) | `PENDENTE` |
| S5.3 | Server `VisionMind`: detecção (Haar/DNN) + identificação (Ollama vision) + presença | pipeline v1 DM4.9 reproduzido: PRESENT/LEFT_RECENTLY corretos em bancada | `PENDENTE` |
| S5.4 | Gaze tracking (rosto→`STIMULUS`+gaze target) + away detection (→ sono) | olhos seguem rosto em tempo real; ausência 60 s → sono; retorno → despertar | `PENDENTE` |
| S5.5 | Enrollment + ativação de perfil (persona local do v1) | cadastro pelo dashboard; reconhecimento ativa perfil; tudo local | `PENDENTE` |
| S5.6 | Gate de visão | soak 24 h com face loop ativo; recovery limpo ao cobrir/descobrir câmera e reiniciar server | `PENDENTE` |

### S6 — Movimento (servos sob safety)

*Objetivo:* pescoço expressivo com a disciplina de safety do v1.
*Dependências:* S0.1, S3.6. **Bloqueio absoluto:** S6.2+ não inicia sem S6.1
assinado.

| ID | Entrega | Gate de saída | Status |
| --- | --- | --- | --- |
| S6.1 | **Gate elétrico assinado** (checklist físico: proteção reversa, fuse por trilho, isolação da trilha servo, GND, brownout sob stall com fonte limitada) | checklist em `docs/bringup/` com fotos e medições; assinado antes de qualquer torque no robô | `PENDENTE` |
| S6.2 | `servo_hal` 1 fio (S0.1 promovido a produção pelo padrão P3) + `motion_safety` portado do v1 (stall/temp/subtensão/heartbeat/brownout, FAULT pegajoso) **com host-test do núcleo** | host-tests de veto/fault/idempotência; em bancada: stall induzido → torque-off < 150 ms | `PENDENTE` |
| S6.3 | `motion_service` (interpolação, primitivos de pescoço, heartbeat p/ safety, limites por config NVS) | movimento suave centro↔limites; posição fora de range vetada (log) | `PENDENTE` |
| S6.4 | Integração expressiva: gaze físico + gestos curtos coordenados com face (conductor mínimo) | linguagem corporal do v1 reproduzida; retorno limpo ao centro em toda entrada em IDLE (invariante estendida a pescoço) | `PENDENTE` |
| S6.5 | Gate de movimento | soak 48 h com movimento periódico; injeção de stall/brownout → FAULT correto e recuperação por reset; zero evento de safety perdido | `PENDENTE` |

### S7 — Produto (paridade v1 e release)

*Objetivo:* fechar paridade com o v1 e cortar a primeira release.
*Dependências:* S5.6 (S6 pode correr em paralelo; release não bloqueia em S6
se servos atrasarem — produto funciona sem pescoço).

| ID | Entrega | Gate de saída | Status |
| --- | --- | --- | --- |
| S7.1 | Status rail (ícones persistentes: mic, câmera, mente offline, SD, safety) + quick status | estados simultâneos alinhados; zero disputa com a face; honesto sob falha | `PENDENTE` |
| S7.2 | `PersonaMind` + memória longa (SQLite; perfil, preferências, fatos; wipe pelo dashboard) | continuidade entre sessões; wipe completo verificado | `PENDENTE` |
| S7.3 | Dashboard v2 (React): chat, status, métricas do robô, enrollment, provisioning, wipe | fluxos principais utilizáveis; métricas plotadas do `STATUS` | `PENDENTE` |
| S7.4 | `SkillHost`: agenda cognitiva sobre `schedule_core`, device commands, busca web opcional | timer por voz fim-a-fim; busca degradável sem chave | `PENDENTE` |
| S7.5 | **Soak de release: 7 dias** + smoke assinado (`QUALITY.md` §6) + auditoria de segurança (`SECURITY.md` §5) | 7 dias sem crash, heap estável; checklists arquivados em `docs/releases/v2.0/` | `PENDENTE` |
| S7.6 | Release v2.0: tag, OTA assinada publicada, docs revisadas, v1 aposentado formalmente | robô do dia a dia rodando v2; repo v1 arquivado como referência | `PENDENTE` |

## 5. Dependências entre fases (resumo)

```
S0 ──► S1.6+ ──► S1.9 ──► S2 ──► S3 ──► S4 ──► S5 ──► S7
 │       (S1.1–S1.5 podem                 ▲            ▲
 │        correr em paralelo a S0)        │            │
 └────────► S6.1 ──► S6.2–S6.5 ───────────┴── paralelo ┘
            (após S3.6; nunca antes do gate elétrico)
```

## 6. Fora de escopo do v2.0 (registrado para não vazar)

| Item | Condição de retorno |
| --- | --- |
| IMU / bateria / touchscreen / touch 3 zonas | pós-v2.0, entram por I2C sem mudar pinout |
| AEC / full-duplex conversacional | quando houver meta medida de conversa contínua |
| Wake word customizada | quando o fluxo atual justificar treino próprio |
| TLS no firmware | não retorna (decisão de arquitetura; ver SECURITY.md) |
| HIL com a Waveshare | opcional pós-v2.0; nunca dependência |
| Voz bilíngue / RAG / biblioteca local | herdam os planos do v1 depois da paridade |
