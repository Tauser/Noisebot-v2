# NoiseBot 2 — A Mente (server)

Especificação do server Python. Herança: o server do v1 é a parte mais
aproveitável do projeto — este documento define o **refactor dirigido** (não
reescrita) que o traz para o NB2, e os contratos que os 5 atores obedecem.

Stack: Python ≥ 3.10, aiohttp, SQLite. Roda no PC do usuário (GPU para
STT/LLM/visão). Dashboard: React/Vite.

## 1. Papel e princípios

- A mente faz tudo que exige **linguagem, memória ou raciocínio**; nunca
  participa de safety, render ou reflexos (isso é o corpo — `BEHAVIOR.md`).
- **Local e privada por default**: STT, LLM local (Ollama/LM Studio), TTS e
  visão rodam na máquina do usuário. LLM online por API é opcional e sempre
  degradável; nuvem em tools continua explícita.
- **Descartável em runtime**: a mente pode cair a qualquer momento; o corpo
  degrada suave (`fade ≤ 300 ms → IDLE + ícone`). Nenhum estado do corpo
  depende de resposta da mente.
- **Dado externo é hostil**: transcrição, anexos, resultados de busca e
  conteúdo de documentos nunca dão instruções ao agente (política herdada do
  v1, mantida à letra).

## 2. Topologia interna

```
NBP/2 (TCP 9100) ◄─► Transport ─► EventBus interno ─► 5 atores ─► Stores (SQLite)
HTTP/WS (dashboard) ◄─► OpsAPI ──┘                      │
                                                        └─► Providers (STT/LLM/TTS/Visão)
```

**Regra estrutural (anti-god-object):** nenhum ator chama outro ator — toda
comunicação cruza o EventBus interno. O `Orchestrator` de 2,5k LOC do v1 é o
anti-padrão de referência; sua *lógica* é boa e migra fatiada.

| Ator | Responsabilidade | Origem no v1 |
| --- | --- | --- |
| `TurnEngine` | FSM do turno de voz (§3), deadlines, barge-in, métricas | TurnManager + fases 3–5 do orchestrator |
| `MindOutput` | sentencização, TTS, cache, agendamento de SAY_*, cancelamento | OutputScheduler + fase 6 |
| `VisionMind` | (adiado com a câmera) snapshot→detecção→identidade→presença | vision/ do v1 |
| `PersonaMind` | perfil, humor cognitivo, `STIMULUS` ao corpo, memória longa | persona_sync + LTM tools |
| `SkillHost` | tools da LLM, intents locais, agenda cognitiva, busca | intents + tools/ do v1 |

Transport e OpsAPI são bordas finas: decodificam (codegen NBP/2 / HTTP),
publicam eventos tipados, e nada mais.

## 3. O turno de voz (TurnEngine)

FSM herdada do v1 (invariantes numeradas — viram testes):

```
IDLE ─wake─► LISTENING ─fim de fala─► COMMITTING ─transcrição─► THINKING
  ▲                                                                │
  │◄────────── SpeechDone / TurnError / deadline ◄─── SPEAKING ◄───┘
```

- **I-1** Um turno ativo por vez; turno novo cancela o anterior por completo.
- **I-2** Barge-in (voz ou toque durante SPEAKING) cancela a task do turno
  imediatamente (grace de ~0,9 s pós-início de fala contra eco).
- **I-3** Deadline global do turno (30 s) e deadline de progresso em fala
  (20 s): estouro → `TurnError` → IDLE, nunca FSM presa.
- **I-4** Falha de provider degrada com resposta honesta ("não consegui
  pensar agora") — nunca silêncio sem estado.
- **I-5** Ordem de resolução: **intent local primeiro** (offline-first),
  LLM depois. Intents (hora, timer, status, volume…) respondem em < 1 s sem
  nenhum provider.
- Métricas por turno (herdadas): `stt_ms`, `llm_first_token_ms`,
  `tts_first_audio_ms`, `first_audio_out_ms`, `end_of_turn_ms`.

## 4. Providers

Cada provider atrás de interface própria, com **circuit breaker** (falhas
consecutivas → aberto → half-open com sonda), e a regra de ouro do asyncio:
**nenhum I/O bloqueante no event loop** — CPU-bound e rede síncrona via
`asyncio.to_thread`; subprocessos via `asyncio.create_subprocess_exec`
(nunca `shell=True`).

| Provider | Implementação inicial | Notas herdadas do v1 |
| --- | --- | --- |
| STT | faster-whisper (`medium/cpu/int8`; alvo CUDA medido depois) | falha CUDA nunca degrada silenciosa p/ CPU; STT indisponível é estado explícito |
| LLM | Ollama local (default) / LM Studio local / APIs online (`chat/completions` ou Anthropic) | orçamento de contexto explícito (`num_ctx`/`max_tokens`); anexos longos não podem comer a janela |
| TTS | Piper PT-BR + cache de sentenças | sentencizer alimenta MindOutput por frase (latência) |
| Visão | (adiado) OpenCV pré-filtro + Ollama vision | volta com a fase S5 |

Configuração inicial do LLM em runtime:

- `NOISEBOT_LLM_PROVIDER=ollama|lmstudio|openai|api_chat|anthropic`
- `NOISEBOT_LLM_MODEL=<modelo>`
- `NOISEBOT_OLLAMA_BASE_URL` e `NOISEBOT_OLLAMA_NUM_CTX` para local via Ollama
- `NOISEBOT_LMSTUDIO_BASE_URL` para local via LM Studio (`/v1`)
- `OPENAI_API_KEY` + `NOISEBOT_OPENAI_BASE_URL` para GPT e compatíveis OpenAI oficiais
- `NOISEBOT_API_CHAT_BASE_URL` + `NOISEBOT_API_CHAT_API_KEY` + `NOISEBOT_API_CHAT_LABEL` para qualquer API compatível com `chat/completions`
- `ANTHROPIC_API_KEY` + `NOISEBOT_ANTHROPIC_BASE_URL` para APIs no formato Anthropic `messages`

Configuração inicial do TTS/STT em runtime:

- `NOISEBOT_TTS_PROVIDER=none|piper`
- `NOISEBOT_PIPER_EXECUTABLE` + `NOISEBOT_PIPER_MODEL` para TTS local via Piper
- `NOISEBOT_STT_PROVIDER=none|faster_whisper`
- `NOISEBOT_FASTER_WHISPER_MODEL`, `NOISEBOT_FASTER_WHISPER_DEVICE`, `NOISEBOT_FASTER_WHISPER_COMPUTE_TYPE`
- `NOISEBOT_FASTER_WHISPER_CPU_THREADS`, `NOISEBOT_FASTER_WHISPER_NUM_WORKERS`, `NOISEBOT_FASTER_WHISPER_LANGUAGE`, `NOISEBOT_FASTER_WHISPER_BEAM_SIZE`
- `NOISEBOT_STT_TIMEOUT_S` para teto operacional do provider de transcrição

Avanço parcial de `S4.6` (2026-07-09):

- `TurnEngine` agora nasce com `LocalIntentResolver` real por default, sem
  depender de callback de teste para o caminho offline-first.
- Regras locais já cobrem hora, data e status em PT-BR, todas sem tocar LLM.
- `timer` com duração explícita e `alarme` por horário absoluto já são
  resolvidos localmente, publicam `TimerSetRequested` e alcançam a borda
  NBP/2; `volume` com percentual explícito e `modo silencioso` também já
  publicam eventos tipados locais e seguem o mesmo caminho até o firmware.
- `timer` local já pode ser criado na mente por regra PT-BR simples e virar
  evento interno tipado; nesta fatia ele também já alcança a borda NBP/2 do
  server, que responde `HELLO_ACK`/`TIME_SYNC`/`HEARTBEAT` e envia `TIMER_SET`
  ao robô conectado.
- `MindRuntime.from_env()` injeta um status operacional mínimo com o trio
  configurado (`LLM`/`STT`/`TTS`) para a resposta de `status`.
- Evidência de host fica no pytest desta fase (`test_local_intents.py` +
  cobertura de integração do `TurnEngine`). Desde 2026-07-10, a suíte também
  mede a latência observável `FinalTranscript -> SentenceReady/SpeechDone`
  para intent local sem LLM e exige `< 1 s`.
- `MindOutput` também publica `TurnBudgetReported` por turno com
  `speech_to_first_audio_ms`, `reply_to_first_audio_ms` e `end_of_turn_ms`,
  preparando o gate operacional de `S4.7` sem depender ainda do soak longo.
- A mente também ganhou `provider_smoke.py`: um smoke host-side que exercita
  `LLM` → `TTS` → `STT` pelos contratos reais dos providers e devolve
  resultado explícito por eixo, útil para validar configuração de `S4.5`
  sem hardware. Desde 2026-07-10, ele também roda como comando real
  (`python -m noisebot2.provider_smoke [--json]`), montando providers via
  `.env`, fechando `ClientSession`/recursos ao final e retornando `exit 0`
  só quando os eixos solicitados passam. No estágio atual de `S4.5`, isso
  permite smoke parcial por `--only llm|tts|stt` para registrar evidência
  incremental sem mascarar eixos ainda não ligados. Para `TTS`, o smoke aceita
  `--text`; para `STT`, aceita `--wav` mono PCM16LE quando não houver `TTS`
  gerando áudio no mesmo fluxo.

## 5. SkillHost — tools e intents

- **Intents locais** (regex/regras PT-BR, zero LLM): hora/data e status do
  sistema já respondem localmente na fatia atual de `S4.6`; `timer` com
  duração explícita e `alarme` por horário absoluto também resolvem antes do
  LLM e publicam `TimerSetRequested` para o corpo. `volume` com valor
  explícito (`volume 40`) publica `VolumeSetRequested`, e `modo silencioso`
  publica `QuietModeSetRequested`; ambos seguem por NBP/2 até o firmware.
  Consultas vagas ou ajustes relativos ainda devolvem guidance honesta local,
  sem cair em LLM. Esse é o piso offline atual da voz e ele sempre resolve
  antes de qualquer provider (I-5).
- **Tools da LLM** (two-step loop herdado: LLM pede tool → executa → LLM
  conclui), catálogo inicial portado do v1: `set_expression`,
  `show_message`, `create_timer`, `create_reminder`, `list_agenda`,
  `remember_fact`, `forget_fact`, `recall_user_preferences`, `web_search`
  (opcional, Tavily, degradável sem chave).
- Regras: toda tool valida argumentos por schema; resultado de tool é dado
  não confiável; tools que tocam o corpo passam pelo EventBus (viram
  `EXPRESSION_HINT`/`TIMER_SET`/etc. — nunca chamada direta ao transport).
- Agenda cognitiva: `SkillHost` cria/cancela via NBP/2 `TIMER_SET` — o
  **corpo é o dono do disparo** (funciona offline); a mente guarda o
  contexto ("lembrar de quê") na camada episódica.

## 6. Stores

Detalhe completo em `MEMORY.md`. Resumo de propriedade:

- `ConversationStore` (SQLite, desenho C0 do v1): turnos, conversas,
  idempotência, paginação, recuperação de contexto ("vamos continuar").
- `PersonaStore`: perfil declarado + push de snapshot ao corpo (idempotente
  por versão).
- `FactStore`: fatos/preferências com origem e data; wipe por camadas.
- Migrations versionadas; banco em `~/.noisebot2/`; backup = copiar arquivo.

## 7. Dashboard

React/Vite servido pelo OpsAPI. Herda o ciclo multimodal do v1 (validado):

- chat com anexos — imagem ≤ 5 MB, documento (PDF/DOCX/TXT) ≤ 10 MB, áudio
  ≤ 20 MB; validação por conteúdo; **anexos só em memória** (~30 min),
  nunca em disco; citações por página/parágrafo/intervalo;
- `response_mode`: `dashboard` (resposta só na UI, robô livre) ou `robot`
  (fluxo normal de voz) — isolamento herdado à letra;
- telas: chat, status do robô (espelho do `STATUS` NBP/2 + métricas),
  histórico de conversas, perfil/persona, provisioning, wipe por camadas;
- o browser nunca fala com o robô — sempre via server.

## 8. Segurança operacional

Herdada do v1 (o lado que já estava correto) + endurecimentos:

- OpsAPI: bind `127.0.0.1` por default; POST sempre com Bearer token
  timing-safe (`secrets.compare_digest`); allowlist de IP para acesso
  externo; token em `~/.noisebot2/ops_token` (0600) ou env.
- NBP/2: valida token do HELLO do robô (obrigatório — sem modo opt-out).
- Secrets só por env, lidos no ponto de uso; config expõe apenas
  `*_configured` booleans; `secrets-scan` no CI.
- Anexos e busca: tamanho/formato validados por conteúdo; SSRF: a busca não
  abre URLs (leitura profunda só com proteção real, decisão futura).

## 9. Observabilidade

- `/metrics` (JSON): métricas de turno, circuit breakers, fila de eventos,
  uptime, e o espelho do `STATUS` do corpo (heap/fps/drops) — série curta
  persistida para o dashboard plotar.
- Logs estruturados por ator; nível por env; nunca conteúdo de
  áudio/transcrição em log de produção.

## 10. Testes

- pytest (matriz 3.10/3.11 no CI, sem filtro de paths).
- **Fake firmware** (herdado do v1, peça-chave): um cliente NBP/2 simulado
  que faz handshake, envia EVENT/LISTEN e recebe SAY — todos os testes de
  turno rodam sem hardware.
- Golden tests do protocolo compartilhados com o firmware (codegen).
- Cada invariante I-1…I-5 tem teste nomeado (`test_turn_i2_barge_in_...`).

## 11. Estrutura do pacote

```
server/noisebot2/
├── transport/        # NBP/2 server + codegen wrapper (borda fina)
├── ops/              # OpsAPI http + security + static do dashboard
├── mind/             # 5 atores (turn_engine, mind_output, vision_mind,
│                     #            persona_mind, skill_host)
├── providers/        # stt, llm, tts (+ circuit breaker comum)
├── stores/           # conversation, persona, facts (SQLite + migrations)
├── bus.py            # EventBus interno tipado
└── runtime.py        # composição/boot dos atores
```

## 12. Fases de entrada (referência ao ROADMAP)

| Peça | Fase |
| --- | --- |
| Transport NBP/2 + fake firmware + golden | S1.7 |
| OpsAPI mínima + segurança + /metrics | S1.7–S1.9 |
| TurnEngine + MindOutput + providers + intents | S4.4–S4.6 |
| ConversationStore | S4.4 |
| Dashboard chat/status | S7.3 |
| PersonaMind + FactStore + wipe | S7.2 |
| SkillHost completo (tools + agenda + busca) | S7.4 |
| VisionMind | pós-v2.0 (com a câmera) |
