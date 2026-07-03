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
| `ADIADO` | Fora do escopo v2.0 por decisão registrada; pinos/contratos reservados |

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
| Fase atual | S1 — fundação (S1.4 e S1.5 `FEITO`; S1.3 bloqueado por S0.3/SD físico); S0 corre em paralelo |
| Próximo marco | Pinout congelado (S0.4, tag `pinout-v1.0`) |
| Hardware | **Waveshare N32R16V única** (decisão 2026-07-01); SD externo; Freenove segue rodando o v1. Rota alternativa Freenove preservada em `HARDWARE_FREENOVE.md` |
| Câmera | **ADIADA** (decisão 2026-07-02): form factor estilo StackChan não tem cavidade; slot SPI (CS 9/MISO 13) e mensagens `SNAPSHOT_*` reservados |
| Servo | **Perfil B inicial** (decisão 2026-07-02): MG90S PWM em 17/18 + INA219 (stall por corrente) + corte MOSFET (GPIO3); upgrade perfil A (SCS0009/TTLinker) nos mesmos pinos — ver `HARDWARE.md` §Perfis de motion |
| Maior risco atual | S0.3 (contenção render+áudio+SD) nunca foi medido em nenhuma das gerações |
| Regra de ouro | CI verde é pré-condição de merge desde S1.1 |

## 4. Fases e subfases

### S0 — Spikes de bancada e congelamento de pinout

*Objetivo:* provar as três hipóteses de hardware que o design assume.
*Dependências:* nenhuma. *Procedimentos completos:* `S0_BRINGUP.md`.
*Regra:* código de spike vive fora da árvore de produto (`scratch/spikes/`) e
morre depois; nenhuma linha dele é promovida sem reescrita pelo padrão P3.

| ID | Entrega | Gate de saída | Status |
| --- | --- | --- | --- |
| S0.1 | Health check da N32R16V + display ST7789 no SPI2 (IO-MUX 10/11/12/14) | MAC/medições elétricas registradas; 1 h a 30 fps sem artefato; PSRAM 16 MB reconhecida; frequência final registrada | `PENDENTE` |
| S0.2 | Câmera SPI no barramento compartilhado (CS 9, MISO 13) | JPEG íntegro 100/100; fps ≥ 28 durante capturas; zero erro de barramento em 30 min | `ADIADO` (junto com S5; executa se/quando a câmera voltar) |
| S0.3 | microSD externo (SDMMC 6/15/16) + contenção total render+áudio+SD | zero underrun em 30 min; fps ≥ 28; escrita SD nunca bloqueia áudio/render; PSRAM livre ≥ 10 MB | `PENDENTE` |
| S0.4 | Pinout congelado | `HARDWARE.md` sem marcador SPIKE; evidências em `docs/bringup/`; tag `pinout-v1.0` | `PENDENTE` |

### S1 — Fundação (infra + segurança + CI)

*Objetivo:* esqueleto executável com toda a infraestrutura de qualidade e
segurança — antes de qualquer feature de produto.
*Dependências:* S0.4 (apenas para S1.6+; S1.1–S1.5 podem começar em paralelo
ao S0). *Camadas:* L0 parcial, L1, início do mind_link.

| ID | Entrega | Gate de saída | Status |
| --- | --- | --- | --- |
| S1.1 | Repo + CI completo (`QUALITY.md` §1): build `-Werror`, host-tests, lint, secrets-scan, budget-gates (tetos iniciais) | CI verde no primeiro `main.c`; PR de teste com warning proposital fica vermelho | `FEITO` |
| S1.2 | `event_bus` (pool estático, slots de safety, fila de safety, ring de auditoria) **com teste de burst no mesmo commit** | host-test: zero drop não-safety sob perfil de burst alvo; safety imune a fila cheia | `FEITO` |
| S1.3 | `logger` estruturado (ring RAM + worker SD) + dump de ring em shutdown **e panic** (coredump partition) | panic forçado em bancada produz coredump legível + ring de eventos | `EM ANDAMENTO` |
| S1.4 | `config` (NVS tipada, chaves centralizadas) + `boot_manager` por fases com relatório | boot < 3 s até task idle; falha de fase crítica → SAFE_MODE testado | `FEITO` |
| S1.5 | `watchdog` (TWDT + HW) integrado a todas as tasks existentes | task travada em bancada → reset + causa registrada em NVS | `FEITO` |
| S1.6 | WiFi + **provisioning SoftAP** (SSID/senha via app oficial Espressif; token entra em S1.7 — ver ajuste de escopo registrado abaixo) | provisionar do zero pelo celular sem toolchain; `secrets-scan` confirma zero credencial no repo | `EM ANDAMENTO` |
| S1.7 | NBP/2 núcleo: codegen do `nbp2.yaml` (C+Python), framing/CRC32, HELLO+token timing-safe, HEARTBEAT, TIME_SYNC, EVENT, STATUS, reconexão com backoff | golden tests C↔Python no CI; HELLO sem/erro de token → conexão encerrada (teste dos dois lados); soak de reconexão 100 ciclos | `PENDENTE` |
| S1.8 | OTA A/B assinada + anti-rollback + Secure Boot v2 + flash encryption (chaves geridas por `SECURITY.md` §3) | OTA ida-e-volta em bancada; imagem adulterada recusada; dump de flash não revela token; procedimento de recuperação de chave documentado | `PENDENTE` |
| S1.9 | Soak do esqueleto | 24 h: zero reset, heap estável, reconexões limpas com server de teste | `PENDENTE` |

**Evidência S1.1 (2026-07-02):**

- Build local: `idf.py build` verde via ambiente ESP-IDF v5.5.4 do `CLAUDE.md`
  (`export.bat` + `idf.py build`), binário `noisebot2.bin` com 95% livre na
  partição app.
- CI no PR #2 (`S1.1: corrigir gates iniciais do CI`) verde em `firmware-build`,
  `secrets-scan`, `server-tests (3.10)` e `server-tests (3.11)`; PR mergeado em
  `main`.
- Prova negativa no PR #1 (`S1.1: provar gate Werror`): warning proposital
  derrubou `firmware-build` com `unused variable` tratado como erro por
  `-Werror`; PR fechado sem merge.
- Gates pendentes fora do escopo S1.1: budgets finais de SRAM/PSRAM/fps ficam
  para os gates das fases que criarem carga real (`S0.3`, `S2.1`, `S2.6`).

**Plano S1.2 (antes de implementar):**

1. Criar `event_bus` em núcleo C17 puro (`event_bus.c/.h`) sem FreeRTOS/ESP-IDF,
   com clock/I/O injetados quando necessário.
2. Usar pool estático com tipos `nb_*`, slots reservados para safety, fila
   prioritária de safety e ring de auditoria de tamanho fixo.
3. Definir política explícita de overflow: evento safety nunca perde slot para
   tráfego normal; não-safety deve cumprir zero drop no perfil de burst alvo.
4. Adicionar `host_test` de burst no mesmo commit de implementação, cobrindo
   saturação normal, reserva safety, ordem de entrega e auditoria.
5. Integrar ao build/CI sem publicar eventos de HAL diretamente; camadas acima
   consomem o bus conforme `ARCHITECTURE.md`.

**Evidência S1.2 (2026-07-02):**

- Implementado `firmware/components/infra/event_bus` como núcleo C17 puro,
  sem FreeRTOS/ESP-IDF/malloc, com pool estático de 32 slots, 4 reservados
  para safety, fila safety separada e ring de auditoria.
- Host-test de burst no mesmo commit: cobre zero drop normal no perfil alvo,
  fila normal cheia com safety aceito, prioridade safety na entrega, auditoria
  de drop/poll e payload inválido.
- Gate local: `python tools/run_host_tests.py` verde; `idf.py build` verde
  compilando `event_bus`; `tools/scan_secrets.py` verde; `git diff --check`
  verde.
- CI no PR #3 verde em `firmware-build`, `host-tests`, `secrets-scan`,
  `server-tests (3.10)` e `server-tests (3.11)`.
- Gates pendentes fora do escopo S1.2: casca concorrente/task dona do bus e
  integração com logger/panic entram em S1.3+; HAL continua proibido de
  publicar diretamente.

**Plano S1.3 (antes de implementar):**

1. Criar `logger` em núcleo C17 puro (`logger.c/.h`), mesmo padrão do
   `event_bus`: sem FreeRTOS/ESP-IDF/malloc, pool estático, clock injetado
   (timestamp passado pelo chamador, não lido internamente).
2. Ring de entradas estruturadas (nível, módulo, timestamp, mensagem de
   tamanho fixo) com número de sequência monotônico por entrada.
3. Duas formas de leitura do ring, sem remover conceitualmente os dados:
   snapshot completo (para dump de shutdown/panic) e leitura incremental por
   cursor de sequência (`drain_since`, para o futuro worker SD), com detecção
   explícita de gap quando o worker fica para trás e o ring sobrescreve
   entradas ainda não persistidas.
4. Filtro de nível (`min_level`) e truncamento seguro de mensagem (sem
   estourar buffer, sempre terminada em `NUL`).
5. `host_test` no mesmo commit cobrindo ordenação, filtro, overflow/drop,
   `drain_since` incremental e gap, truncamento.
6. **Fora do escopo desta fatia, registrado para não vazar silenciosamente:**
   a casca (task FreeRTOS, mutex de serialização, worker SD, hook de dump em
   shutdown/panic) fica para depois, porque depende de (a) S0.3 — microSD
   ainda `PENDENTE`, sem HAL de sdmmc na árvore — e (b) confirmação em
   bancada de qual mecanismo do ESP-IDF v5.5.4 injeta dados de aplicação no
   coredump antes de um panic real; não implementar esse hook sem validar,
   para não criar uma falsa sensação de gate cumprido.

**Evidência S1.3 (2026-07-02, parcial):**

- Implementado `firmware/components/infra/logger` como núcleo C17 puro, sem
  FreeRTOS/ESP-IDF/malloc: ring estático de 128 entradas, sequência
  monotônica, filtro de nível, `nb_logger_copy_all` (snapshot completo) e
  `nb_logger_drain_since` (leitura incremental com detecção de gap).
- Host-test no mesmo commit: ordenação oldest→newest, filtro de nível,
  overflow com contagem de drop, `drain_since` incremental em duas chamadas,
  detecção de gap quando o cursor fica para trás do overwrite, truncamento
  seguro de mensagem longa.
- Gate local (sandbox, sem toolchain ESP-IDF disponível): `python3
  tools/run_host_tests.py` verde compilando `event_bus` e `logger`.
- Gate local confirmado na máquina de desenvolvimento (2026-07-02): `idf.py
  build` verde, compilando `logger` e gerando `noisebot2.bin` (95% livre na
  partição app); `python tools/scan_secrets.py` verde (`secrets-scan:
  limpo`); `git diff --check` verde (só avisos de normalização LF→CRLF, sem
  erro de whitespace).
- Commit `S1.3: nucleo do logger` na branch `codex/s1.2-event-bus`.
- Casca FreeRTOS adicionada (`shell/nb_logger_shell.c/.h`): singleton
  protegido por mutex (`xSemaphoreCreateMutex`) para uso seguro por múltiplas
  tasks, timestamp real via `esp_timer_get_time()`. Sem task dedicada (não há
  worker para alimentar ainda) e sem worker SD/hook de panic — deferidos por
  P5 e pelo bloqueio abaixo.
- Gate local confirmado na máquina de desenvolvimento (2026-07-02): `idf.py
  build` verde com a casca nova, gerando `noisebot2.bin` (95% livre na
  partição app); `python tools/scan_secrets.py` verde (`secrets-scan:
  limpo`); `python3 tools/run_host_tests.py` verde (núcleo não foi tocado).
- **Gate pendente (bloqueia `FEITO`):** o gate de saída de S1.3 em si —
  "panic forçado em bancada produz coredump legível + ring de eventos" —
  que exige: (1) worker SD (depende de S0.3 — aguardando módulo microSD
  físico), (2) hook de panic validado em hardware real. Status permanece
  `EM ANDAMENTO`; próxima fatia entra quando o módulo microSD estiver em mãos
  para destravar S0.3/worker SD, ou quando o hook de panic for confirmado
  isoladamente em bancada.

**Plano S1.4 (antes de implementar):**

1. `config`: núcleo C17 puro (`config.c/.h`), mesmo padrão dos componentes
   anteriores — sem FreeRTOS/ESP-IDF/malloc. Tabela estática central de
   chaves tipadas (`nb_config_key_t`: id, nome, tipo, default, min/max para
   numéricos) — fonte única da verdade, nenhuma chave solta em módulo
   separado. Cache em RAM validado contra o schema: `get`/`set` com
   validação de tipo e faixa antes de aceitar; `set` fora de faixa é
   rejeitado sem corromper o valor anterior.
2. Conjunto inicial de chaves mínimo para provar o mecanismo (nível de log,
   contador de falhas de boot consecutivas para o `boot_manager`); chaves de
   features futuras (WiFi, etc.) entram quando a feature existir — nada de
   chave especulativa.
3. `host_test` no mesmo commit: get/set válido, rejeição fora de faixa,
   valor default quando nunca setado, tipo errado rejeitado.
4. Casca (`shell/`) fica para o commit seguinte: persistência real em NVS
   (`nvs_flash`), leitura na inicialização, escrita só quando o valor muda.
   Sem isso o "tipada" fica só em RAM — documentado como pendente, não
   escondido.
5. `boot_manager`: núcleo C17 puro (`boot_manager.c/.h`) separado, também sem
   FreeRTOS — sequência de fases nomeadas com criticidade
   (`CRITICAL`/`NON_CRITICAL`), clock injetado (duração por fase). Decide
   `BOOT_OK` vs `SAFE_MODE` (qualquer fase crítica falhou) e monta relatório
   (fase, sucesso, duração, causa). `host_test` cobrindo: soma de duração,
   fase não-crítica falha sem acionar `SAFE_MODE`, fase crítica falha aciona
   `SAFE_MODE` e marca a causa, relatório determinístico.
6. Casca do `boot_manager` (orquestrar init real de `logger`/`config`/
   `event_bus` em sequência, medir boot→idle < 3 s) fica para depois do
   núcleo de ambos os componentes estar prontos — próxima fatia desta mesma
   subfase, não uma nova.

**Evidência S1.4 (2026-07-02):**

- Implementado `firmware/components/infra/app_config` como núcleo C17 puro,
  sem FreeRTOS/ESP-IDF/malloc/NVS: tabela central de chaves tipadas
  (`nb_config_key_t`/`nb_config_descriptor_t`), getters/setters tipados por
  chave (`_u32`/`_i32`) que rejeitam tipo errado ou valor fora de faixa sem
  alterar o valor anterior, default quando a chave nunca foi setada. Chaves
  iniciais: `NB_CONFIG_KEY_LOG_MIN_LEVEL` e `NB_CONFIG_KEY_BOOT_FAIL_STREAK`.
- Componente nomeado `app_config` (pasta e arquivos), não `config`: o
  ESP-IDF já tem um componente interno chamado `config` (geração de
  `sdkconfig.h`); o build silenciosamente ignorou nossa primeira versão
  chamada `config` sem erro, e só a renomeação para `app_config` fez o
  `idf.py build` de fato compilar o componente próprio — registrado aqui
  para quem for nomear componente novo não cair na mesma armadilha.
- Host-test no mesmo commit: default sem set prévio, set válido lido de
  volta, set fora de faixa rejeitado mantendo o valor anterior, accessor do
  tipo errado rejeitado, chave inválida rejeitada em todas as funções.
- Gate local confirmado na máquina de desenvolvimento (2026-07-02): `idf.py
  build` verde compilando `__idf_app_config` (`libapp_config.a`),
  `noisebot2.bin` com 95% livre na partição app; `python3
  tools/run_host_tests.py` verde (`app_config`, `event_bus`, `logger`);
  `python tools/scan_secrets.py` verde (`secrets-scan: limpo`).
- Implementado `firmware/components/infra/boot_manager` como núcleo C17
  puro, sem FreeRTOS/ESP-IDF/malloc: sequência de fases nomeadas
  (`begin_phase`/`end_phase`) com criticidade, capacidade fixa de 16 fases,
  relatório determinístico (fase, criticidade, sucesso, duração, duração
  total). Outcome `BOOT_OK`/`SAFE_MODE`: qualquer fase crítica que falhe vira
  `SAFE_MODE` com o nome da fase como motivo pegajoso (não sobrescrito por
  falhas seguintes); falha em fase não-crítica não afeta o outcome.
- Host-test no mesmo commit: soma de duração e outcome OK com todas as fases
  passando, falha não-crítica não aciona SAFE_MODE, falha crítica aciona
  SAFE_MODE com motivo correto, motivo pegajoso na segunda falha crítica,
  `end_phase` sem `begin_phase` rejeitado, `begin_phase` sobre fase ainda
  aberta rejeitado, overflow de capacidade rejeitado.
- Gate local confirmado na máquina de desenvolvimento (2026-07-02): `idf.py
  build` verde compilando `__idf_app_config` e `__idf_boot_manager`
  (`libapp_config.a`, `libboot_manager.a`), `noisebot2.bin` com 95% livre na
  partição app; `python3 tools/run_host_tests.py` verde (`app_config`,
  `boot_manager`, `event_bus`, `logger`); `python tools/scan_secrets.py`
  verde (`secrets-scan: limpo`).
- Casca do `app_config` adicionada (`shell/nb_app_config_shell.c/.h`):
  singleton com mutex, `nvs_flash_init()` (erase+retry se corrompida/versão
  nova), namespace `nb_cfg`; carrega cada chave do NVS na inicialização
  (ausente = default) e só escreve (`nvs_set_*` + `nvs_commit`) quando o
  valor validado muda.
- Casca do `boot_manager` adicionada (`shell/nb_boot_manager_shell.c/.h`):
  `nb_boot_manager_shell_run()` orquestra, em sequência, `logger` (crítico) e
  `app_config` (crítico) com duração medida por `esp_timer`; em SAFE_MODE
  incrementa `NB_CONFIG_KEY_BOOT_FAIL_STREAK` (melhor esforço), em BOOT_OK
  zera o contador. `event_bus` fica de fora da sequência de propósito — a
  casca dele só nasce quando houver serviço publicando evento (ver seu
  README), não é esquecimento.
- `firmware/main/main.c` chama `nb_boot_manager_shell_run()` no início do
  `app_main` e loga outcome/fases/duração total.
- Gate local confirmado na máquina de desenvolvimento (2026-07-02): `idf.py
  build` verde compilando `__idf_app_config`, `__idf_boot_manager` e
  `__idf_main` com as cascas novas, `noisebot2.bin` com 94% livre na
  partição app; `python3 tools/run_host_tests.py` verde (núcleos
  inalterados: `app_config`, `boot_manager`, `event_bus`, `logger`); `python
  tools/scan_secrets.py` verde (`secrets-scan: limpo`).
- **Ensaio em bancada (2026-07-02, N32R16V via COM5):** flash real e captura
  serial (115200) confirmam boot→`app_main` em ~1,27 s e as duas fases do
  `boot_manager` somando 30 ms (`outcome=1 fases=2 duracao_total_ms=30`) —
  bem dentro do gate de < 3 s. Falha crítica forçada de propósito (fase
  `app_config` marcada como falha, revertido antes do commit): log real
  `E boot_manager: SAFE_MODE: fase 'app_config' falhou` e
  `boot: outcome=2 fases=2 duracao_total_ms=30` (2=`NB_BOOT_OUTCOME_SAFE_MODE`).
  **Gate de saída da subfase atendido.** Status `FEITO`.

**Plano S1.5 (antes de implementar):**

1. Criar `watchdog` em núcleo C17 puro (`watchdog.c/.h`), sem FreeRTOS,
   ESP-IDF, NVS ou `malloc`: cadastro fixo de tasks, último feed, timeout por
   task e causa de reset observada.
2. Adicionar host-test no mesmo commit cobrindo feed normal, task expirada,
   rejeição de duplicidade, limite de capacidade, argumentos inválidos e
   armazenamento da causa de reset no núcleo.
3. Criar casca FreeRTOS/ESP-IDF (`shell/nb_watchdog_shell.c/.h`) inicializando
   o TWDT com panic/reset, assinando a task chamadora e incluindo as idle tasks
   no `idle_core_mask`, para cobrir travas que impedem escalonamento.
4. Persistir em NVS própria do watchdog (`nb_wdog`) o `esp_reset_reason()` e a
   causa classificada no boot seguinte. A chave tipada no `app_config` fica
   para depois de S1.4 fechar, para não acoplar duas subfases em andamento.
5. Integrar apenas as tasks existentes hoje: `app_main` + idle tasks do ESP-IDF.
   Cada task nova de S2+ deve entrar no watchdog no commit em que nascer.

**Evidência S1.5 (2026-07-02):**

- Implementado `firmware/components/infra/watchdog` como núcleo C17 puro:
  tabela fixa de 16 tasks, nomes truncados com `NUL`, feed por `task_id`,
  detecção determinística da primeira task expirada e causa de reset guardada
  no estado do núcleo.
- Casca `shell/nb_watchdog_shell.c/.h`: inicializa TWDT com `trigger_panic`,
  monitora idle tasks dos dois cores via `idle_core_mask`, assina `app_main` e
  grava em NVS (`nb_wdog/last_reset`, `nb_wdog/last_cause`) a causa observada
  após reboot (`TASK_WDT`, `INT_WDT` ou `WDT` genérico).
- Integração mínima no `app_main`: `nb_watchdog_shell_init(10000)` após o boot
  manager e `nb_watchdog_shell_feed()` a cada 1 s no loop `alive`.
- Host-test no mesmo commit: feed normal, expiração, duplicidade, capacidade,
  argumentos inválidos e causa de reset.
- Gate local confirmado: `python tools/run_host_tests.py` verde
  (`app_config`, `boot_manager`, `event_bus`, `logger`, `watchdog`);
  `python tools/scan_secrets.py` verde (`secrets-scan: limpo`);
  `git diff --check` sem erro de whitespace (apenas avisos LF→CRLF no Windows);
  `idf.py build` verde via ESP-IDF v5.5.4, compilando `__idf_watchdog` e
  gerando `noisebot2.bin` com 94% livre na menor partição app.
- **Achado de bancada corrigido antes do gate:** `sdkconfig` tem
  `CONFIG_ESP_TASK_WDT_INIT=y` (TWDT do sistema já ativa antes do nosso
  `app_main`) com `CONFIG_ESP_TASK_WDT_PANIC` **desligado**. A versão inicial
  da casca aceitava o `ESP_ERR_INVALID_STATE` de `esp_task_wdt_init()` e
  seguia sem reconfigurar — nossa escolha de `trigger_panic=true` nunca era
  aplicada, e uma task travada de verdade não reiniciaria o robô. Corrigido
  chamando `esp_task_wdt_reconfigure()` nesse caso.
- **Ensaio em bancada (2026-07-02, N32R16V via COM5):** versão temporária do
  `app_main` parou de alimentar o watchdog de propósito (revertida antes do
  commit). Log real: após 10 s sem feed, `task_wdt: Task watchdog got
  triggered ... Aborting`, coredump salvo em flash, reboot automático; no
  boot seguinte, `reset_reason=6` (`ESP_RST_TASK_WDT`) e
  `watchdog: TWDT ativo: ... reset_anterior=1`
  (`NB_WATCHDOG_RESET_CAUSE_TASK_TIMEOUT`) — causa persistida em NVS e lida
  de volta corretamente após o reset. **Gate de saída da subfase atendido.**
  Status `FEITO`.

**Ajuste de escopo S1.6 (2026-07-02, antes de implementar):** a entrega
original lista "SSID/senha/token → NVS". O token NBP/2 só faz sentido
semântico quando o protocolo nascer em S1.7 (ele é usado no HELLO, `nbp2.yaml`
ainda não existe) — implementá-lo agora seria inventar um formato de token
sem contrato. Além disso, o app oficial "ESP SoftAP Provisioning" da
Espressif (usado para cumprir "sem toolchain" no celular) fala WiFi
nativamente, mas não envia dado customizado sem um cliente protocomm próprio
— colocar o token aqui atritaria com o próprio gate "sem toolchain". Escopo
desta subfase: só SSID/senha via SoftAP. Token entra em S1.7, junto do
handshake HELLO, onde o transporte (endpoint protocomm customizado vs.
reaproveitar o próprio HELLO) pode ser decidido com o contrato do protocolo
em mãos.

**Plano S1.6 (antes de implementar):**

1. Componente `wifi_setup` (nome evita colisão com os componentes internos
   `wifi_provisioning`/`protocomm` do próprio ESP-IDF — mesma lição do
   `app_config`/`config`).
2. Núcleo C17 puro (`wifi_setup.c/.h`): validação de SSID (1–32 bytes, sem
   `NUL` embutido) e senha (vazia = rede aberta, ou 8–63 chars ASCII
   imprimíveis por WPA2-PSK), estado determinístico do fluxo
   (`NOT_PROVISIONED` → `PROVISIONING` → `PROVISIONED`/`FAILED`). Sem
   FreeRTOS, NVS ou rede — só validação e transição de estado.
3. `host_test` no mesmo commit: SSID vazio/grande demais rejeitado, senha
   curta demais rejeitada, senha vazia aceita (rede aberta), transições de
   estado válidas e inválidas.
4. Casca (`shell/`): `wifi_prov_mgr` do ESP-IDF com transporte SoftAP,
   `WIFI_PROV_SECURITY_1` (PoP fixo, sem TLS — compatível com a regra de
   "rede é LAN local"), SSID do AP derivado do MAC (`NoiseBot2-XXXX`). WiFi
   já persiste SSID/senha nativamente via `esp_wifi` (`WIFI_STORAGE_FLASH`) —
   não duplicar em `app_config` (que também não suporta string ainda).
5. Gate real ("provisionar do zero pelo celular sem toolchain") exige o app
   oficial da Espressif rodando num celular contra a N32R16V — não é
   executável sem essa etapa manual do usuário; fica pendente até acontecer.

### S2 — Face (o robô fica vivo, mudo)

*Objetivo:* display + renderer + FSM + idle. No fim de S2 o robô parece vivo.
*Dependências:* S1.9. *Referência de implementação:* renderer do head v1 (DM2).

| ID | Entrega | Gate de saída | Status |
| --- | --- | --- | --- |
| S2.1 | `display_hal` (ST7789 SPI 50 MHz, 3 pinos, double buffer PSRAM, wrapper `extern "C"`) | padrão de teste a 30 fps por 1 h; zero artefato; SRAM inalterada (gate do `.map`) | `PENDENTE` |
| S2.2 | Renderer paramétrico (10 expressões de `VISUAL.md` §2, interpolação 220 ms, AA sub-pixel) | paridade visual com v1 confirmada lado a lado; fps ≥ 30 medido | `PENDENTE` |
| S2.3 | `tiny_fsm` (8 estados + modos, `BEHAVIOR.md` §1) **nascendo com o teste de invariante X→IDLE** | host-test cobre 100% das transições × modos; invariante verde | `PENDENTE` |
| S2.4 | `idle_engine` (catálogo de motifs de `VISUAL.md` §3: blink Poisson, curious tilt, head tilt, look-down) | critério de 60 s de `VISUAL.md` §3 atendido em bancada; parâmetros documentados | `PENDENTE` |
| S2.5 | `emotion_core` v0 (vetor+decaimento+âncoras, `BEHAVIOR.md` §2) modulando neutral/idle | host-test de decaimento, clamp e integração de estímulo; efeito visível em bancada | `PENDENTE` |
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
| S4.2 | `wake_service` (WakeNet) + VAD (ESP-SR) com invariantes V-1..V-6 de `VOICE.md` §3 **como host-tests** | wake em ambiente real ≥ 9/10; falso-wake < 1/h; overlay listening < 250 ms; testes V-* verdes | `PENDENTE` |
| S4.3 | Streaming NBP/2 de áudio (LISTEN_* robô→server; SAY_* server→robô; canal MEDIA com backpressure; barge-in físico por touch) | golden tests; sessão completa contra server fake; queda de link no meio da fala → fade ≤ 300 ms + IDLE | `PENDENTE` |
| S4.4 | Server: `TurnEngine` + `MindOutput` extraídos do orchestrator v1 (atores sobre bus, nenhum ator chama outro) | testes de turno portados do v1 passam na nova estrutura; barge-in cancela task de turno | `PENDENTE` |
| S4.5 | Providers ligados: faster-whisper, Ollama/OpenAI com circuit breaker, Piper | conversa fim-a-fim em PT-BR; falha de LLM degrada com resposta honesta, sem travar FSM | `PENDENTE` |
| S4.6 | Intents locais offline-first (hora, timer, status) respondendo sem LLM | intents respondem com LLM desligada; latência < 1 s | `PENDENTE` |
| S4.7 | Gate de voz | budgets §4 de `QUALITY.md` medidos e registrados (wake→listening, fala→primeiro áudio); soak 24 h com conversas periódicas | `PENDENTE` |

### S5 — Visão (presença e identidade) — **FASE ADIADA**

*Decisão 2026-07-02:* câmera fora do escopo v2.0 — o form factor estilo
StackChan não tem cavidade para o módulo (~33×33×17 mm; só a lente passa por
um furo, mas o corpo precisa de espaço interno atrás dele). Slot elétrico
(CS GPIO9, MISO 13), mensagens `SNAPSHOT_*` e capability no HELLO permanecem
reservados. Presença no v2.0 degrada para som (VAD/análise), wake, touch e
sono por inatividade. Rotas de retorno registradas: ArduCam Mega M12 atrás
de janela, ou câmera WiFi independente (ex.: Freenove aposentada) falando
direto com o server.

*Objetivo (quando voltar):* câmera sob demanda + pipeline semântico na mente.
*Dependências:* S4.7 + S0.2 + decisão mecânica. *Referência:* DM4/13.1 do v1.

| ID | Entrega | Gate de saída | Status |
| --- | --- | --- | --- |
| S5.1 | `camera_hal` (ArduCam Mega SPI, JPEG sob demanda; driver pelo padrão núcleo/casca sobre o spike S0.2) | captura estável; zero interferência em render/áudio (contadores) | `PENDENTE` |
| S5.2 | `SNAPSHOT_*` no NBP/2 (chunks MEDIA, transfer_id, CRC, preempção por CTRL) | JPEG íntegro no server < 400 ms p95; controle nunca atrasado por transferência (medido) | `PENDENTE` |
| S5.3 | Server `VisionMind`: detecção (Haar/DNN) + identificação (Ollama vision) + presença | pipeline v1 DM4.9 reproduzido: PRESENT/LEFT_RECENTLY corretos em bancada | `PENDENTE` |
| S5.4 | Gaze tracking (rosto→`STIMULUS`+gaze target) + away detection (→ sono) | olhos seguem rosto em tempo real; ausência 60 s → sono; retorno → despertar | `PENDENTE` |
| S5.5 | Enrollment + ativação de perfil (persona local do v1) | cadastro pelo dashboard; reconhecimento ativa perfil; tudo local | `PENDENTE` |
| S5.6 | Gate de visão | soak 24 h com face loop ativo; recovery limpo ao cobrir/descobrir câmera e reiniciar server | `PENDENTE` |

### S6 — Movimento (servos sob safety)

*Objetivo:* pescoço expressivo com a disciplina de safety do v1, no
**perfil B** (MG90S provisório — `HARDWARE.md` §Perfis de motion).
*Dependências:* S3.6. **Bloqueio absoluto:** S6.2+ não inicia sem S6.1
assinado. Trilho B com fuse, INA219 e MOSFET de corte; 5V próprio, GND
comum, nunca o 5V da placa dev.
*Upgrade perfil A (SCS0009 + TTLinker):* não é fase nova — é troca da casca
do HAL (`servo_hal_scs`, núcleo do protocolo SCS herdado do v1) +
re-execução dos gates S6.2 e S6.5 em bancada. Registrar no painel quando
ocorrer.

| ID | Entrega | Gate de saída | Status |
| --- | --- | --- | --- |
| S6.1 | **Gate elétrico assinado** (checklist de `ENERGY.md` §4: proteção reversa, fuse por trilho, isolação 5V↔3V3, GND estrela, brownout sob stall) | checklist em `docs/bringup/` com fotos e medições; assinado antes de qualquer torque no robô | `PENDENTE` |
| S6.2 | `servo_hal` como **interface dupla** (caps `has_feedback`): casca `servo_hal_pwm` (LEDC 17/18) + `motion_safety` com perfis por capability (B: stall via INA219 + corte MOSFET GPIO3; temp coberta por limite de duty) **com host-test do núcleo** | host-tests de veto/fault/idempotência/perfil; em bancada: eixo bloqueado → corte do trilho < 150 ms | `PENDENTE` |
| S6.3 | `motion_service` (interpolação, primitivos de pescoço, heartbeat p/ safety, limites por config NVS, **detach em repouso** — zero PWM/zumbido parado) | movimento suave centro↔limites; posição fora de range vetada (log); silêncio audível em idle | `PENDENTE` |
| S6.4 | Integração expressiva: gaze físico + gestos curtos coordenados com face (conductor mínimo) | linguagem corporal do v1 reproduzida; retorno limpo ao centro em toda entrada em IDLE (invariante estendida a pescoço) | `PENDENTE` |
| S6.5 | Gate de movimento | soak 48 h com movimento periódico; injeção de stall/brownout → FAULT correto e recuperação por reset; zero evento de safety perdido | `PENDENTE` |

### S7 — Produto (paridade v1 e release)

*Objetivo:* fechar paridade com o v1 (exceto visão, adiada) e cortar a
primeira release.
*Dependências:* S4.7 (S6 pode correr em paralelo; release não bloqueia em S6
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
S0 ──► S1.6+ ──► S1.9 ──► S2 ──► S3 ──► S4 ──► S7
        (S1.1–S1.5 podem                  ▲       ▲
         correr em paralelo a S0)         │       │
S3.6 ──► S6.1 ──► S6.2–S6.5 ──────────────┴─ paralelo ┘
         (nunca antes do gate elétrico assinado)
S5 (visão): ADIADA — retorna após v2.0 com S0.2 + decisão mecânica
```

## 6. Fora de escopo do v2.0 (registrado para não vazar)

| Item | Condição de retorno |
| --- | --- |
| Câmera / visão (fase S5) | pós-v2.0; **slot reservado** (CS GPIO9, MISO 13, `SNAPSHOT_*` no schema); exige decisão mecânica (cavidade ou câmera WiFi externa) |
| Tela touch | pós-v2.0; **pinos já reservados** (ctrl no I2C 4/5, INT no GPIO1) |
| IMU MPU-6050 | pós-v2.0; **pinos já reservados** (I2C 4/5, INT no GPIO8) |
| Bateria (charger/boost/fuel gauge) | pós-v2.0; I2C + revisão do orçamento de energia na entrada |
| Touch 3 zonas (MPR121) | pós-v2.0; I2C, custo zero de GPIO |
| AEC / full-duplex conversacional | quando houver meta medida de conversa contínua |
| Wake word customizada | quando o fluxo atual justificar treino próprio |
| TLS no firmware | não retorna (decisão de arquitetura; ver SECURITY.md) |
| HIL com a Waveshare | opcional pós-v2.0; nunca dependência |
| Voz bilíngue / RAG / biblioteca local | herdam os planos do v1 depois da paridade |
