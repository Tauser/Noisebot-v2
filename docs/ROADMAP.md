# NoiseBot 2 — Roadmap Canônico

**Status:** ativo
**Autoridade:** este documento define ordem, IDs, dependências e gates da
construção do NoiseBot 2. `CLAUDE.md` define regras de código;
`ARCHITECTURE.md` define a arquitetura-alvo. Em divergência de status,
**este arquivo prevalece**.

## 1. Como ler

| Status         | Significado                                                            |
| -------------- | ---------------------------------------------------------------------- |
| `PENDENTE`     | Ainda não iniciada                                                     |
| `EM ANDAMENTO` | Trabalho ativo com ID declarado                                        |
| `FEITO`        | Todos os critérios de saída atendidos, com evidência registrada        |
| `BLOQUEADO`    | Aguardando dependência explícita                                       |
| `FALLBACK`     | Concluída pela alternativa documentada (registrar qual)                |
| `ADIADO`       | Fora do escopo v2.0 por decisão registrada; pinos/contratos reservados |

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

| Campo             | Decisão                                                                                                                                                                                                        |
| ----------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Fase atual        | S1 — fundação (S1.1/S1.2/S1.4/S1.5/S1.6/S1.7 `FEITO`; S1.3 bloqueado por S0.3/SD físico; S1.8 OTA em andamento); S0 corre em paralelo                                                                          |
| Próximo marco     | Pinout congelado (S0.4, tag `pinout-v1.0`)                                                                                                                                                                     |
| Hardware          | **Waveshare N32R16V única** (decisão 2026-07-01); SD externo; Freenove segue rodando o v1. Rota alternativa Freenove preservada em `HARDWARE_FREENOVE.md`                                                      |
| Câmera            | **ADIADA** (decisão 2026-07-02): form factor estilo StackChan não tem cavidade; slot SPI (CS 9/MISO 13) e mensagens `SNAPSHOT_*` reservados                                                                    |
| Servo             | **Perfil B inicial** (decisão 2026-07-02): MG90S PWM em 17/18 + INA219 (stall por corrente) + corte MOSFET (GPIO3); upgrade perfil A (SCS0009/TTLinker) nos mesmos pinos — ver `HARDWARE.md` §Perfis de motion |
| Maior risco atual | S0.3 (contenção render+áudio+SD) nunca foi medido em nenhuma das gerações                                                                                                                                      |
| Regra de ouro     | CI verde é pré-condição de merge desde S1.1                                                                                                                                                                    |

## 4. Fases e subfases

### S0 — Spikes de bancada e congelamento de pinout

_Objetivo:_ provar as três hipóteses de hardware que o design assume.
_Dependências:_ nenhuma. _Procedimentos completos:_ `S0_BRINGUP.md`.
_Regra:_ código de spike vive fora da árvore de produto (`scratch/spikes/`) e
morre depois; nenhuma linha dele é promovida sem reescrita pelo padrão P3.

| ID   | Entrega                                                               | Gate de saída                                                                                                       | Status                                                     |
| ---- | --------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------- |
| S0.1 | Health check da N32R16V + display ST7789 no SPI2 (IO-MUX 10/11/12/14) | MAC/medições elétricas registradas; 1 h a 30 fps sem artefato; PSRAM 16 MB reconhecida; frequência final registrada | `PENDENTE`                                                 |
| S0.2 | Câmera SPI no barramento compartilhado (CS 9, MISO 13)                | JPEG íntegro 100/100; fps ≥ 28 durante capturas; zero erro de barramento em 30 min                                  | `ADIADO` (junto com S5; executa se/quando a câmera voltar) |
| S0.3 | microSD externo (SDMMC 6/15/16) + contenção total render+áudio+SD     | zero underrun em 30 min; fps ≥ 28; escrita SD nunca bloqueia áudio/render; PSRAM livre ≥ 10 MB                      | `PENDENTE`                                                 |
| S0.4 | Pinout congelado                                                      | `HARDWARE.md` sem marcador SPIKE; evidências em `docs/bringup/`; tag `pinout-v1.0`                                  | `PENDENTE`                                                 |

### S1 — Fundação (infra + segurança + CI)

_Objetivo:_ esqueleto executável com toda a infraestrutura de qualidade e
segurança — antes de qualquer feature de produto.
_Dependências:_ S0.4 (apenas para S1.6+; S1.1–S1.5 podem começar em paralelo
ao S0). _Camadas:_ L0 parcial, L1, início do mind_link.

| ID   | Entrega                                                                                                                                             | Gate de saída                                                                                                                            | Status         |
| ---- | --------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------- | -------------- |
| S1.1 | Repo + CI completo (`QUALITY.md` §1): build `-Werror`, host-tests, lint, secrets-scan, budget-gates (tetos iniciais)                                | CI verde no primeiro `main.c`; PR de teste com warning proposital fica vermelho                                                          | `FEITO`        |
| S1.2 | `event_bus` (pool estático, slots de safety, fila de safety, ring de auditoria) **com teste de burst no mesmo commit**                              | host-test: zero drop não-safety sob perfil de burst alvo; safety imune a fila cheia                                                      | `FEITO`        |
| S1.3 | `logger` estruturado (ring RAM + worker SD) + dump de ring em shutdown **e panic** (coredump partition)                                             | panic forçado em bancada produz coredump legível + ring de eventos                                                                       | `EM ANDAMENTO` |
| S1.4 | `config` (NVS tipada, chaves centralizadas) + `boot_manager` por fases com relatório                                                                | boot < 3 s até task idle; falha de fase crítica → SAFE_MODE testado                                                                      | `FEITO`        |
| S1.5 | `watchdog` (TWDT + HW) integrado a todas as tasks existentes                                                                                        | task travada em bancada → reset + causa registrada em NVS                                                                                | `FEITO`        |
| S1.6 | WiFi + **provisioning SoftAP** (SSID/senha via app oficial Espressif; token entra em S1.7 — ver ajuste de escopo registrado abaixo)                 | provisionar do zero pelo celular sem toolchain; `secrets-scan` confirma zero credencial no repo                                          | `FEITO`        |
| S1.7 | NBP/2 núcleo: codegen do `nbp2.yaml` (C+Python), framing/CRC32, HELLO+token timing-safe, HEARTBEAT, TIME_SYNC, EVENT, STATUS, reconexão com backoff | golden tests C↔Python no CI; HELLO sem/erro de token → conexão encerrada (teste dos dois lados); soak de reconexão 100 ciclos            | `FEITO`        |
| S1.8 | OTA A/B assinada + anti-rollback + Secure Boot v2 + flash encryption (chaves geridas por `SECURITY.md` §3)                                          | OTA ida-e-volta em bancada; imagem adulterada recusada; dump de flash não revela token; procedimento de recuperação de chave documentado | `EM ANDAMENTO` |
| S1.9 | Soak do esqueleto                                                                                                                                   | 10 min: zero reset, heap estável, reconexões limpas com server de teste                                                                  | `PENDENTE`     |

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

**Evidência S1.6 (2026-07-02):**

- Implementado `firmware/components/infra/wifi_setup` como núcleo C17 puro:
  validação de SSID/senha e máquina de estados `NOT_PROVISIONED` →
  `PROVISIONING` → `PROVISIONED`/`FAILED`, `reset` de qualquer estado.
  Host-test cobrindo validação e todas as transições.
- Casca `shell/nb_wifi_setup_shell.c/.h`: `wifi_prov_mgr` (ESP-IDF) sobre
  SoftAP, `WIFI_PROV_SECURITY_1`, SSID do AP `NoiseBot2-XXXX` derivado do
  MAC, PoP fixo `nb2setup` (limitação conhecida — documentada no README,
  revisitar antes de produção). Eventos do `wifi_prov_mgr` alimentam a
  máquina de estados do núcleo. Integrado no `app_main` após o watchdog;
  falha aqui loga e segue offline, não trava o boot (P1).
- Gate local confirmado na máquina de desenvolvimento: `idf.py build` verde
  (`__idf_wifi_setup`, `noisebot2.bin` com 78% livre — queda esperada pela
  stack WiFi/protocomm linkada); `python3 tools/run_host_tests.py` verde;
  `python tools/scan_secrets.py` verde (`secrets-scan: limpo`).
- **Ensaio em bancada (2026-07-02, N32R16V via COM5):** a placa já tinha
  credenciais WiFi persistidas na NVS de uso anterior (fora do nosso
  controle). Log real confirma o caminho "já provisionado":
  `wifi_setup: ja provisionado, conectando em modo estacao`, seguido de
  `wifi:mode : sta` — o branch de reconexão direta funciona. O caminho
  SoftAP/PoP (dispositivo não provisionado) não foi testado nesta sessão
  para não apagar a NVS compartilhada com `app_config`/`watchdog` sem
  necessidade — decisão do usuário.
- **Ensaio em bancada com reset de provisioning (2026-07-02, N32R16V via
  COM5):** NVS de WiFi apagada (`esptool erase_region 0x9000 0x6000`) para
  forçar estado não-provisionado. Log real confirma o SoftAP subindo:
  `wifi_setup: nao provisionado: SoftAP 'NoiseBot2-3D58', PoP 'nb2setup'`,
  `wifi:mode : sta + softAP`, `wifi_setup: provisioning iniciado`.
  Provisionamento real feito do zero pelo app oficial "ESP SoftAP
  Provisioning" (Android) contra `NoiseBot2-3D58`/PoP `nb2setup`, com SSID e
  senha da rede do usuário — sem toolchain, só o celular. Após reboot
  seguinte, log confirma persistência: `wifi_setup: ja provisionado,
conectando em modo estacao` — as credenciais aplicadas pelo app foram
  gravadas na NVS e sobrevivem a reset. **Gate de saída da subfase
  atendido.** `secrets-scan` confirma zero credencial no repo (PoP/SSID/
  senha nunca tocam o código-fonte). Status `FEITO`.

**Plano S1.7 (antes de implementar):**

1. Começar pelo envelope do protocolo, não pela rede: codegen a partir de
   `protocol/nbp2.yaml` para C17 e Python, com IDs de mensagem, versão,
   framing `SOF|len|type|seq|payload|crc32` e comparação timing-safe de token.
2. Tratar o payload como bytes opacos nesta primeira fatia. Payloads CBOR e
   structs por mensagem entram na próxima fatia da mesma subfase, depois que
   o envelope estiver protegido por golden test executável.
3. Adicionar job `protocol-golden` ao CI no mesmo commit do codegen: gerar os
   artefatos, compilar o helper C no host, gerar os mesmos frames em Python e
   comparar bytes reais, nunca regex sobre fonte.
4. Manter `protocol/generated/` como saída de build ignorada pelo git; a fonte
   de verdade versionada continua sendo o YAML + codegen.
5. Deixar TCP/reconexão/backoff e teste de HELLO real contra server fake para
   as próximas fatias de S1.7, porque dependem do encoder/decoder de payload.

**Evidência S1.7 (2026-07-02 a 2026-07-03):**

- Adicionado `protocol/codegen/generate_nbp2.py`: parser mínimo do
  `nbp2.yaml` que valida IDs duplicados e gera `protocol/generated/c/nbp2.h`,
  `protocol/generated/c/nbp2.c` e `protocol/generated/python/nbp2.py`.
- Artefatos gerados incluem `NBP2_PROTO_MAJOR/MINOR`, `NBP2_SOF`,
  `NBP2_MAX_CTRL_PAYLOAD`, enum/constantes dos 26 IDs de mensagem do YAML,
  `encode_frame`, `decode_frame`, CRC32 IEEE e comparação timing-safe de
  token em C/Python. Núcleo C17 sem FreeRTOS, ESP-IDF ou `malloc`.
- Adicionado `tools/check_protocol_golden.py`: regenera os artefatos,
  compila um binário C host usando os arquivos gerados e compara contra o
  módulo Python gerado os bytes de frames `HELLO` e `HEARTBEAT`, além dos
  resultados de token igual, token diferente e comprimento diferente.
- CI atualizado com job `protocol-golden` sem filtro de paths, seguindo
  `QUALITY.md` §1.
- Gate local confirmado: `python tools/check_protocol_golden.py` verde
  (`nbp2-codegen: 26 mensagens geradas`, `protocol-golden: ok`);
  `python tools/run_host_tests.py` verde; `python tools/scan_secrets.py`
  verde; `git diff --check` sem erro de whitespace (apenas avisos LF→CRLF no
  Windows).
- **Payloads CBOR (2026-07-02):** `generate_nbp2.py` reescrito para usar
  PyYAML (única dependência externa do toolchain; `pip install pyyaml` no
  job `protocol-golden`) em vez do parser regex — schema completo (enums,
  campos tipados) precisava de um parser de verdade, não regex sobre YAML.
  Gera agora, para as 26 mensagens: struct C (`nbp2_msg_<nome>_t`) e
  dataclass Python por mensagem, `nbp2_encode_*`/`nbp2_decode_*` em C e
  `encode_*`/`decode_*` em Python, usando CBOR canônico (array posicional de
  campos, RFC 8949 forma curta) implementado à mão nos dois lados — sem
  `cbor2` nem lib de terceiros, para que os bytes baterem por construção.
  Tipos suportados (os únicos usados no YAML hoje): u8/u16/u32/u64/i8/f32/
  bytes(max)/str(max)/enum. Structs de mensagem renomeados para
  `nbp2_msg_<nome>_t` (não `nbp2_<nome>_t`) depois que a mensagem `STATUS`
  colidiu com o enum de erro `nbp2_status_t` já existente — mesma família de
  armadilha do `app_config`/`config`, registrada para não repetir. Campo
  `from` (EVENT*STATE) é palavra reservada em Python: o gerador renomeia só
  no lado Python (`from*`) via `keyword.iskeyword`, mantendo `from` no C.
- `tools/check_protocol_golden.py` estendido: além do frame/token já
  cobertos, agora codifica HELLO/HEARTBEAT/STATUS/TIMER_SET/EVENT_STATE em C
  com os mesmos valores usados em Python e compara os bytes CBOR; e decodifica
  em C bytes que o Python codificou (HELLO/STATUS/TIMER_SET), provando as
  duas direções, não só round-trip dentro da mesma linguagem. Esses cinco
  mensagens cobrem todos os 9 tipos de campo do schema.
- Gate local confirmado: `python tools/check_protocol_golden.py` verde
  (`nbp2-codegen: 26 mensagens geradas`, `protocol-golden: ok`); compilação
  do C gerado com `-Wall -Wextra -Werror` sem warning; `python3
tools/run_host_tests.py` verde; `python tools/scan_secrets.py` verde.
- **Validação de token do HELLO (2026-07-02):** escopo decidido
  explicitamente — a validação fica no nível de protocolo (C+Python, golden
  test), sem NVS nem integração no build do ESP-IDF ainda. Persistência real
  em NVS e o transporte TCP dependem do `mind_link` (serviço L3,
  `ARCHITECTURE.md`), que ainda não existe; integrar o codegen (Python +
  PyYAML) no CMake do firmware antes disso teria risco real de quebrar o job
  `firmware-build` do CI sem um consumidor real do lado do robô.
  `tools/check_protocol_golden.py` ganhou 3 casos: token correto, token
  errado (mesmo tamanho) e token ausente — decodifica o frame completo
  (envelope + payload HELLO) e valida com `nbp2_timing_safe_equal`, nos dois
  lados. Sanity check manual confirmou que o teste realmente pega divergência
  (alterei um valor esperado de propósito e o gate ficou vermelho, revertido
  antes do commit).
- Gate local confirmado: `python tools/check_protocol_golden.py` verde
  (token correto aceito, errado e ausente rejeitados, nos dois lados);
  `python3 tools/run_host_tests.py` verde; `python tools/scan_secrets.py`
  verde.
- **Núcleo do `mind_link` (2026-07-03):** criado
  `firmware/components/services/mind_link` — primeiro componente em
  `components/services/` (camada L3 de `ARCHITECTURE.md`). Núcleo C17 puro
  (`mind_link.c/.h`), sem FreeRTOS/sockets/ESP-IDF: máquina de estados da
  sessão `IDLE` → `AWAITING_HELLO_ACK` → `READY` → `DEAD` (`PROTOCOL.md`
  §3), heartbeat de 1 s com morte após 3 perdidos (literal do §3.3),
  `boot_id` divergente no HELLO_ACK invalida a sessão (§3.4), backoff
  exponencial determinístico de reconexão (`min(500ms·2^tentativa, 30s)`,
  zera no primeiro handshake bem-sucedido). Host-test com 11 casos.
- Achado: `mind_link.c` compilava limpo no host-test (MinGW) mas falhava no
  toolchain do ESP-IDF por falta de `#include <stddef.h>` (uso de `NULL`) —
  o libc do host puxa `stddef.h` transitivamente por outro header, o do
  ESP-IDF não. Lembrete de que host-test não substitui o build real do
  toolchain alvo.
- Gate local confirmado: `idf.py build` verde (`__idf_mind_link`, 78%
  livre — sem mudança de tamanho, componente ainda não linkado no `main`);
  `python3 tools/run_host_tests.py` verde (11/11 casos do `mind_link`);
  `python tools/scan_secrets.py` verde.
- **Componente `nbp2` no build do ESP-IDF (2026-07-03):** resolvido o risco
  registrado antes — criado `firmware/components/infra/nbp2`, casca fininha
  que só compila `protocol/generated/c/nbp2.c/.h` (não gera nada). O codegen
  (Python + PyYAML) roda **fora** do toolchain do ESP-IDF: no CI, o job
  `firmware-build` gera os artefatos com o Python padrão do runner antes de
  chamar a action do ESP-IDF; localmente, é um passo manual documentado no
  README do componente (`python protocol/codegen/generate_nbp2.py`). Se os
  arquivos gerados não existirem, o CMake falha com `message(FATAL_ERROR
...)` explicando o comando a rodar, em vez de um erro de arquivo não
  encontrado difícil de rastrear — testado de propósito (renomeei
  `protocol/generated/` e rodei `idf.py build`: erro claro; restaurado antes
  do commit).
- Gate local confirmado: `idf.py build` verde (`__idf_nbp2`, `noisebot2.bin`
  78% livre — nbp2 ainda não linkado no `main`, só disponível);
  `python3 tools/run_host_tests.py` verde; `python
tools/check_protocol_golden.py` verde; `python tools/scan_secrets.py`
  verde.
- **Casca do `mind_link` com TCP real (2026-07-03):** adicionado
  `shell/nb_mind_link_shell.c/.h` (task FreeRTOS, socket TCP contra
  `CONFIG_NBP2_SERVER_HOST:PORT`, envia HELLO/HEARTBEAT usando `nbp2` +
  reassemblagem de frame com resync em SOF ruim) e
  `shell/nb_mind_link_token_shell.c/.h` (token em NVS, namespace `nbp2_tok`,
  nunca logado). `tools/nbp2_fake_server.py` criado como servidor
  descartável de bancada (não é o server real — server v2 não existe antes
  de S4) para validar o handshake sem precisar da mente.
- **Três bugs reais achados e corrigidos em bancada (N32R16V, COM5):**
    1. **Stack overflow.** Três buffers de ~4 KB (`NBP2_MAX_CTRL_PAYLOAD`)
       viviam na pilha da task (6 KB): o acumulador de recepção, o frame
       parseado e os buffers de envio de HELLO — send_hello chama send_frame,
       os dois ficam vivos ao mesmo tempo. Sintoma real: `Guru Meditation
Error (Cache error)`, `InstrFetchProhibited`, `IllegalInstruction` em
       boot loop, registradores com lixo aleatório. Corrigido tornando os
       quatro buffers `static` (fora da pilha) — só a task usa, chamada
       sequencial, sem problema de reentrância.
    2. **`esp_wifi_connect()` faltando.** `nb_wifi_setup_shell_init()` chamava
       `esp_wifi_start()` no caminho "já provisionado" mas nunca pedia pra
       conectar — a estação nunca associava, `mind_link` ficava com
       "network unreachable" (errno 118) para sempre, sem nenhum erro óbvio
       apontando a causa. Corrigido registrando handler de `WIFI_EVENT_STA_START`
       (chama `esp_wifi_connect()`) e `WIFI_EVENT_STA_DISCONNECTED` (reconecta)
       em `wifi_setup`, mais log de `IP_EVENT_STA_GOT_IP` (útil por si só).
    3. **`SO_RCVTIMEO` sem garantia.** O `recv()` bloqueava para sempre depois
       do servidor ficar em silêncio por um tempo, mesmo com `SO_RCVTIMEO`
       configurado — sintoma: heartbeat parava de vez (contador travado) sem
       nenhum log de erro, task simplesmente presa dentro do `recv()`.
       Corrigido trocando para socket não-bloqueante (`O_NONBLOCK` via
       `fcntl`) com `vTaskDelay` no `EAGAIN`, mecanismo mais robusto que
       depender do timeout do socket.
    4. `getaddrinfo()` trocado por `inet_pton()` direto (host sempre IPv4
       literal nesta fatia) — evita o caminho de resolução DNS do lwip, mais
       pesado em pilha, sem necessidade real ainda (hostname/mDNS não é
       escopo de S1.7).
- **Ensaio em bancada com handshake completo (2026-07-03, N32R16V via
  COM5, `tools/nbp2_fake_server.py`):** log real dos dois lados confirma
  round-trip completo — robô: `HELLO enviado`; server:
  `HELLO boot_id=0x... token_len=0`, `HELLO_ACK enviado, sessao READY`,
  `HEARTBEAT recebido counter=1`, `counter=2`. Sessão estável sem
  reconexão nem crash durante a janela observada. **Ambiente de bancada
  instável**: a rede local (WiFi doméstico + firewall do Windows) produziu
  falhas intermitentes de conexão (`errno 104`/`113`, RST/host unreachable)
  entre tentativas, inclusive na mesma porta que funcionou momentos antes —
  atribuído a instabilidade de associação do AP e/ou firewall do host de
  teste, não ao firmware (nenhum dos bugs corrigidos acima recorreu depois
  de corrigidos). Regra de firewall `TCP 9100 entrada` criada no host de
  teste; teste bem-sucedido usou porta 8765 (mesma do server v1) por
  já ter passagem livre confirmada na rede do usuário.
- Gate local confirmado: `python3 tools/run_host_tests.py` verde (núcleo
  inalterado); `idf.py build` verde com a casca nova; `python
tools/scan_secrets.py` verde.
- **Correção do diagnóstico anterior (2026-07-03):** a instabilidade de rede
  registrada acima **não era WiFi/AP/firewall** — eram processos zumbis do
  próprio `tools/nbp2_fake_server.py` acumulados na mesma porta
  (`pkill -f nbp2_fake_server` nunca matou nada nesse ambiente Windows/
  git-bash; `netstat` mostrou 3 listeners + 2 conexões `ESTABLISHED`
  simultâneas na porta 8765 de execuções anteriores nunca encerradas). Depois
  de `taskkill //F //IM python.exe` limpar tudo, a rede local funcionou de
  primeira e permaneceu estável — não houve mais nenhum `errno 104`/`113`
  depois da limpeza. Fica registrado para não repetir: sempre conferir
  `netstat` antes de culpar a rede.
- **Soak de 100 reconexões (2026-07-03, N32R16V via COM5):**
  `tools/nbp2_fake_server.py` ganhou `--drop-after`/`--max-cycles` para
  automatizar o ciclo — derruba a conexão de propósito a cada 2 s e conta
  quantos ciclos fecham com `HELLO_ACK`. Resultado real:
  `soak concluido: 100/100 ciclos com HELLO_ACK`. Log serial do robô
  confirma uptime contínuo de ~9,8 min (586s+) sem reset nem
  `Guru Meditation`/`stack overflow` durante todo o soak.
- **Rejeição de token em tráfego TCP real (2026-07-03):**
  `tools/nbp2_fake_server.py` ganhou `--require-token HEX`: recusa o HELLO
  (fecha a conexão sem `HELLO_ACK`) quando o token não bate, via
  `nbp2.timing_safe_equal` — o mesmo mecanismo já coberto no golden test,
  agora fim-a-fim sobre socket real. Contra o robô real (token vazio, ainda
  não provisionado): 3/3 HELLOs rejeitados (`soak concluido: 0/3`), sem
  travar nem crashar — o robô trata a recusa como desconexão normal e seguiu
  tentando reconectar com backoff, como esperado.
- **Gate de saída de S1.7 atendido.** Status `FEITO`. Pendente fora do
  escopo desta subfase: provisionamento real do token (endpoint/fluxo de
  configuração) fica para quando o `mind_link` ganhar consumidor real
  (server v2, S4+); a persistência em si (`nb_mind_link_token_shell`) já
  existe e foi exercitada pelo teste de rejeição acima.

**Plano S1.8 (antes de implementar):**

1. Começar pela parte reversível: componente `ota` em L1, com núcleo C17 puro
   para a política de anti-rollback/commit e casca ESP-IDF fina sobre
   `esp_ota_ops`.
2. Integrar no `app_main` a confirmação de imagem OTA pendente somente depois
   que o boot do esqueleto estiver saudável. Falhas não-críticas de WiFi/
   mind_link continuam não bloqueando o robô offline (P1), mas SAFE_MODE não
   confirma imagem nova.
3. Criar perfil separado `sdkconfig.s1_8_secure.defaults` para Secure Boot v2,
   anti-rollback e flash encryption, sem chave no repo e sem ativar eFuse no
   build cotidiano.
4. Documentar recuperação/perda de chave em `SECURITY.md` antes de qualquer
   queima irreversível.
5. Deixar download OTA por NBP/2 para a próxima fatia da S1.8, porque depende
   dos payloads/casca TCP de S1.7 estarem fechados.

**Evidência S1.8 (2026-07-03, parcial):**

- Implementado `firmware/components/infra/ota` como núcleo C17 puro:
  rejeita downgrade de `secure_version`, mantém imagem em estado pendente e
  só permite commit depois de boot marcado como saudável. Sem FreeRTOS,
  ESP-IDF, NVS ou `malloc`.
- Host-test no mesmo commit: aceita versão igual/maior, rejeita downgrade e
  versão fora da janela de eFuse planejada, exige boot saudável antes do
  commit, rejeita commit sem imagem pendente e valida argumentos nulos.
- Casca `shell/nb_ota_shell.c/.h`: lê estado da partição atual com
  `esp_ota_get_state_partition()` e confirma `ESP_OTA_IMG_PENDING_VERIFY`
  com `esp_ota_mark_app_valid_cancel_rollback()` apenas quando o `app_main`
  chega ao ponto saudável. Caminho explícito de rollback/reboot preparado
  para health check futuro.
- `app_main` integrado: se `boot_manager` retorna `NB_BOOT_OUTCOME_OK`,
  confirma imagem OTA pendente; se entra em SAFE_MODE, não confirma.
- Adicionado `firmware/sdkconfig.s1_8_secure.defaults` como perfil separado
  para o gate de bancada, com aviso de eFuses irreversíveis e sem chave
  privada no repo.
- `SECURITY.md` agora inclui procedimento de recuperação/perda de chave antes
  de habilitar Secure Boot v2/flash encryption.
- Gates locais confirmados: `python tools/run_host_tests.py` verde incluindo
  `ota`; `python tools/scan_secrets.py` verde; `git diff --check` sem erro de
  whitespace (apenas avisos LF→CRLF no Windows); `idf.py build` verde via
  ESP-IDF v5.5.4, gerando `noisebot2.bin` com 78% livre na menor partição
  app.
- **Bug real corrigido antes do commit (revisão, 2026-07-03):**
  `nb_ota_accept_candidate` comparava `candidate_secure_version` contra
  `NB_OTA_MAX_SECURE_VERSION_BITS` (8, a **largura em bits** do campo de
  eFuse) como se fosse o valor máximo permitido — um campo de 8 bits
  representa 0..255, então a checagem original rejeitava indevidamente
  quase toda a faixa válida (9..255) como `INVALID_ARG`. O host-test original
  até validava esse comportamento errado como se fosse o esperado. Corrigido
  com `NB_OTA_MAX_SECURE_VERSION = (1u << bits) - 1u`; teste atualizado para
  aceitar o teto real (255) e só rejeitar acima dele (256).
- Gate local reconfirmado após o fix: `idf.py build` verde com `ota`
  realmente recompilado (timestamp do `.o` conferido, não só cache);
  `python tools/run_host_tests.py` verde; `python tools/scan_secrets.py`
  verde.
- **Rollback por software habilitado (2026-07-03):**
  `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` movido para
  `sdkconfig.defaults` (não exige eFuse — estado fica no `otadata`, em
  flash normal). Sem isso, `esp_ota_get_state_partition()` sempre retornava
  `ESP_ERR_NOT_SUPPORTED` e a política de confirmação do `nb_ota_shell`
  nunca era exercitada de verdade.
- **Ensaio de OTA A→B→A em bancada (2026-07-03, N32R16V via COM5,
  `otatool.py` do próprio ESP-IDF):** build v1 flashado em `ota_0`
  (`idf.py flash`, `state=2`/VALID). Build v2 com marca visível no log
  (`[TESTE OTA v2]`, revertida antes do commit) escrito diretamente em
  `ota_1` via `otatool.py write_ota_partition --slot 1`. `switch_ota_partition
  --slot 1` + reset: log real confirma `Loaded app from partition at offset
  0x420000` e a marca da v2 aparece — **A→B confirmado**. `switch_ota_partition
  --slot 0` + reset: log confirma `Loaded app from partition at offset
  0x20000`, marca da v2 sumiu, `state=2` — **B→A confirmado**. Ciclo completo
  sem nenhum crash, usando só partições normais (`ota_0`/`ota_1`/`otadata`),
  nada de eFuse tocado.
- **Nota sobre o gate de "verificação pendente":** `otatool.py` escreve o
  ponteiro de boot direto no `otadata` (ferramenta de baixo nível), sem
  popular o estado `ESP_OTA_IMG_PENDING_VERIFY` que um update real via
  `esp_ota_begin/write/end` deixaria. Por isso o log mostrou `state=-1`
  (`ESP_OTA_IMG_UNDEFINED`) ao carregar a v2, não `state=1`. O caminho de
  confirmação (`nb_ota_shell_confirm_boot_if_pending` marcando
  `PENDING_VERIFY` como válido) só será exercitado de ponta a ponta quando
  existir um download OTA real via NBP/2 (`OTA_BEGIN/CHUNK/END`, fatia
  futura) chamando `esp_ota_set_boot_partition()` depois de um
  `esp_ota_end()` de verdade.
- Gate local confirmado: `python3 tools/run_host_tests.py` verde; `idf.py
  build` verde com rollback habilitado; `python tools/scan_secrets.py`
  verde.
- **Pendente para `FEITO`:** download OTA real via NBP/2 exercitando o
  estado `PENDING_VERIFY` de ponta a ponta; imagem adulterada recusada; dump
  de flash sem token em claro após flash encryption; decisão explícita para
  queima de eFuses da N32R16V — os três últimos exigem o perfil
  `sdkconfig.s1_8_secure.defaults` (irreversível), fora do escopo desta
  fatia de propósito.

### S2 — Face (o robô fica vivo, mudo)

_Objetivo:_ display + renderer + FSM + idle. No fim de S2 o robô parece vivo.
_Dependências:_ S1.9. _Referência de implementação:_ renderer do head v1 (DM2).

**Exceção de ordem registrada (2026-07-03):** S2 está começando com S1.9
ainda `PENDENTE`, por decisão explícita do usuário — S1.9 é um soak de 24h
(zero reset, heap estável, reconexões limpas), não trabalho de código; não
há nada a implementar nele além de deixar rodando e observar. Ficar parado
esperando essas 24h não rende trabalho. Nenhum item de S2 vira `FEITO`
enquanto S1.9 não fechar: o gate de saída de S2 (S2.6, soak 48h) já
pressupõe um esqueleto estável, então a validação final de S2 fica
condicionada, na prática, ao soak de S1.9 ter rodado em algum momento antes
disso. Retomar S1.9 (iniciar o soak de 24h) continua pendente e deve ser
feito antes de considerar S2.6 atendido.

| ID   | Entrega                                                                                                 | Gate de saída                                                                      | Status     |
| ---- | ------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------- | ---------- |
| S2.1 | `display_hal` (ST7789 SPI 50 MHz, 3 pinos, double buffer PSRAM, wrapper `extern "C"`)                   | padrão de teste a 30 fps por 1 h; zero artefato; SRAM inalterada (gate do `.map`)  | `EM ANDAMENTO` |
| S2.2 | Renderer paramétrico (10 expressões de `VISUAL.md` §2, interpolação 220 ms, AA sub-pixel)               | paridade visual com v1 confirmada lado a lado; fps ≥ 30 medido                     | `PENDENTE` |
| S2.3 | `tiny_fsm` (8 estados + modos, `BEHAVIOR.md` §1) **nascendo com o teste de invariante X→IDLE**          | host-test cobre 100% das transições × modos; invariante verde                      | `PENDENTE` |
| S2.4 | `idle_engine` (catálogo de motifs de `VISUAL.md` §3: blink Poisson, curious tilt, head tilt, look-down) | critério de 60 s de `VISUAL.md` §3 atendido em bancada; parâmetros documentados    | `PENDENTE` |
| S2.5 | `emotion_core` v0 (vetor+decaimento+âncoras, `BEHAVIOR.md` §2) modulando neutral/idle                   | host-test de decaimento, clamp e integração de estímulo; efeito visível em bancada | `PENDENTE` |
| S2.6 | Gate visual da fase                                                                                     | soak 48 h face viva sem crash; budgets de fps/PSRAM registrados como baseline      | `PENDENTE` |

**Plano S2.1 (antes de implementar):**

1. Driver ESP-IDF nativo (`esp_lcd_panel_io_spi` + `esp_lcd_new_panel_st7789`),
   C17 puro — decisão explícita para não estender C++/LovyanGFX pra fora da
   camada de renderer (S2.2), respeitando `CLAUDE.md` ao pé da letra. Pinos:
   CS=10, MOSI=11, SCLK=12, DC=14; sem MISO, sem RST dedicado (reset via
   comando SWRESET), sem controle de backlight (BL fixo em 3.3V).
2. Núcleo puro (`display_hal.c/.h`): bookkeeping de double buffer (índice de
   front/back, swap), sem tocar SPI/DMA/ESP-IDF — os ponteiros dos dois
   framebuffers em PSRAM são injetados pelo chamador.
3. Casca (`shell/nb_display_hal_shell.c/.h`): aloca os dois framebuffers em
   PSRAM (`heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` — nunca em SRAM, regra
   de `ARCHITECTURE.md` §6), inicializa o painel via `esp_lcd`, expõe
   `flush()` (`esp_lcd_panel_draw_bitmap`) e delega o buffer swap ao núcleo.
4. `host_test` no mesmo commit cobrindo o núcleo (swap alterna front/back,
   estado inicial determinístico).
5. Padrão de teste visual (cor sólida ou barras) para confirmação humana em
   bancada — não dá pra validar imagem por log, precisa do usuário olhar
   pro display.
6. Gate completo (30 fps por 1h, zero artefato, SRAM inalterada via `.map`)
   é ensaio de bancada prolongado — a fatia de hoje prova que a imagem
   aparece corretamente; o soak de 1h fica registrado como próximo passo.

**Evidência S2.1 (2026-07-03):**

- Implementado `firmware/components/hal/display_hal` — primeiro componente
  em `components/hal/` (camada L0). Núcleo C17 puro (`display_hal.c/.h`):
  bookkeeping de double buffer (índice front/back, swap), sem
  FreeRTOS/SPI/DMA. Host-test cobrindo init determinístico, swap alternando
  front/back, chamadas com `NULL` seguras.
- Casca (`shell/nb_display_hal_shell.c/.h`): ST7789 via `esp_lcd` nativo do
  ESP-IDF (`esp_lcd_panel_io_spi` + `esp_lcd_new_panel_st7789`), C17 puro —
  sem LovyanGFX/C++ nesta camada, por decisão registrada no plano acima.
  Framebuffers (320×240 RGB565) em PSRAM via `heap_caps_malloc(...,
  MALLOC_CAP_SPIRAM)`.
- **Bug real corrigido (2026-07-03):** faltava `esp_lcd_panel_swap_xy(true)`.
  O controlador ST7789 é nativamente 240 (coluna) × 320 (linha); sem trocar
  os eixos, o `draw_bitmap` em paisagem 320×240 mandava endereço de coluna
  até 319 num controlador com só 240 colunas de RAM — causava o artefato
  visual nas duas bordas da tela. Corrigido chamando
  `esp_lcd_panel_swap_xy(panel, true)` no init; independente de velocidade
  de clock (testado e confirmado nas duas pontas).
- **Ensaio real em bancada (2026-07-03, N32R16V via COM5):** sessão de
  debug ao vivo com o usuário, várias hipóteses testadas e descartadas
  metodicamente:
  - Clock 50 MHz (spec do painel) corrompeu cor e apagou a tela nesta
    fiação de jumper — sinal/integridade real, não lógica.
  - Reforço do fio de GND (fio duplicado) eliminou flicker e cor incorreta
    que apareciam mesmo em 40 MHz — indicando contato de GND marginal como
    causa dominante da instabilidade, não o clock em si.
  - Rampa de frequência (1 fps → 5 → 10 → 30 fps) confirmou estabilidade em
    todos os degraus com o padrão estático, depois de corrigido o GND.
  - Com scroll ativo a 30 fps e WiFi/mind_link **desligados** de propósito
    (teste isolado): 30 fps estável por ~30 s, sem flicker, sem troca de
    cor, sem apagar — cores corretas (vermelho/verde/azul/branco/preto).
  - Com WiFi/mind_link religados: tela permanece estável (sem flicker, sem
    apagar), mas com leve troca de cor residual — consistente com
    interferência de RF do rádio WiFi 2.4 GHz acoplando na fiação de jumper
    sem blindagem próxima à antena, não um bug de código.
- Clock inicial pós-GND: **20 MHz** (não os 50 MHz de spec do painel) —
  teto conservador desta fiação de bancada, registrado antes de investigar
  a causa raiz abaixo.
- **Dois bugs reais adicionais corrigidos (2026-07-03), causa raiz — não
  clock nem fiação:**
  - **Race DMA/framebuffer:** `esp_lcd_panel_draw_bitmap()` no SPI é
    assíncrono (enfileira e retorna antes de os pixels saírem); sem
    barreira, a task de render sobrescrevia o framebuffer ainda em
    transmissão, misturando dois frames (flicker). Corrigido com um
    semáforo binário liberado no callback `on_color_trans_done`, que
    serializa: só uma transferência em voo, swap espera o fim da anterior.
  - **Coerência de cache PSRAM:** `esp_lcd_panel_io_spi` (ESP-IDF) nunca
    seta `SPI_TRANS_DMA_USE_PSRAM` nas transações que monta, então
    `spi_master` não sincroniza cache antes do DMA ler um buffer em PSRAM
    (confirmado lendo `esp_driver_spi/src/gpspi/spi_master.c`). O DMA lia
    dado ainda não escrito de volta da cache — corrupção de cor
    intermitente (vermelho→laranja, azul→roxo) independente de clock ou
    fiação. Confirmado em bancada: buffer único em SRAM interna (sem
    PSRAM) ficou perfeito até estático a 10 MHz; buffer em PSRAM sem sync
    manual corrompia mesmo estático. Corrigido com `esp_cache_msync(...,
    ESP_CACHE_MSYNC_FLAG_DIR_C2M)` no back buffer antes de cada
    `draw_bitmap`, exigindo os framebuffers alinhados à linha de cache (32
    bytes, via `heap_caps_aligned_alloc`).
- Clock final: **40 MHz** — confirmado limpo em bancada com as duas
  correções acima; o clock nunca foi a causa raiz. Ainda não é o teto de
  50 MHz de spec do painel; revisitar quando o pinout for congelado
  (S0.4) e a fiação for revisada (par trançado/PCB ao invés de jumper
  solto).
- Gate local confirmado: `python3 tools/run_host_tests.py` verde
  (`display_hal` + núcleos inalterados); `idf.py build` verde (77% livre);
  `python tools/scan_secrets.py` verde.
- **Pendente para `FEITO`:** soak de 30 fps por 1h contínua; teto de clock
  real com fiação definitiva (não jumper); resíduo de cor com WiFi ativo
  (RF/EMI) — revisitar com fiação blindada/PCB no S0.4; budget de SRAM via
  `.map` (framebuffers já em PSRAM, mas a métrica formal não foi extraída
  ainda).

### S3 — Toque, LEDs e reflexos (pet completo offline)

_Objetivo:_ fechar o piso offline do produto.
_Dependências:_ S2.6 (S3.1 pode começar após S2.3).

| ID   | Entrega                                                                                            | Gate de saída                                                                                   | Status     |
| ---- | -------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------- | ---------- |
| S3.1 | `touch_hal` + `touch_service` (calibração do v1 2.2A: threshold 20%, debounce, TAP/LONG/SUSTAINED) | toque intencional 50/50; zero falso positivo em 1 h de ruído ambiente; reproduzível após reboot | `PENDENTE` |
| S3.2 | `reflex_engine` (tabela estímulo→reação com prioridades; touch→afeto integra emotion+face)         | host-test da tabela de arbitragem (conflitos touch×idle×sleep); reação < 80 ms p95 medida       | `PENDENTE` |
| S3.3 | `led_service` (WS2812 no 46; idle/estados/afeto; brilho circadiano)                                | paridade com linguagem de LED do v1; sem flicker                                                | `PENDENTE` |
| S3.4 | Ciclo circadiano + sono (SLEEPING com entrada/saída suaves)                                        | transições dormir/acordar observadas nos horários; invariante IDLE segue verde                  | `PENDENTE` |
| S3.5 | `schedule_core` (timers/alarmes/lembretes locais, persistência NVS, disparo→reflexo+face+led)      | criar/cancelar/disparar OK; reboot não perde nem duplica; disparo com server offline funciona   | `PENDENTE` |
| S3.6 | Gate do piso offline                                                                               | soak 48 h em modo pet (sem server): vivo, responsivo, estável                                   | `PENDENTE` |

### S4 — Voz (o robô conversa)

_Objetivo:_ pipeline de voz fim-a-fim com a mente.
_Dependências:_ S1.7, S3.6. _Referência:_ Voice Audio v2 do v1 (desenho) e
server v1 (refactor).

| ID   | Entrega                                                                                                                       | Gate de saída                                                                                                              | Status     |
| ---- | ----------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------- | ---------- |
| S4.1 | `audio_hal` I2S full-duplex 16 kHz (mic+spk no mesmo barramento)                                                              | loopback limpo; zero underrun em 30 min com render ativo (re-valida S0.3 na árvore real)                                   | `PENDENTE` |
| S4.2 | `wake_service` (WakeNet) + VAD (ESP-SR) com invariantes V-1..V-6 de `VOICE.md` §3 **como host-tests**                         | wake em ambiente real ≥ 9/10; falso-wake < 1/h; overlay listening < 250 ms; testes V-\* verdes                             | `PENDENTE` |
| S4.3 | Streaming NBP/2 de áudio (LISTEN*\* robô→server; SAY*\* server→robô; canal MEDIA com backpressure; barge-in físico por touch) | golden tests; sessão completa contra server fake; queda de link no meio da fala → fade ≤ 300 ms + IDLE                     | `PENDENTE` |
| S4.4 | Server: `TurnEngine` + `MindOutput` extraídos do orchestrator v1 (atores sobre bus, nenhum ator chama outro)                  | testes de turno portados do v1 passam na nova estrutura; barge-in cancela task de turno                                    | `PENDENTE` |
| S4.5 | Providers ligados: faster-whisper, Ollama/OpenAI com circuit breaker, Piper                                                   | conversa fim-a-fim em PT-BR; falha de LLM degrada com resposta honesta, sem travar FSM                                     | `PENDENTE` |
| S4.6 | Intents locais offline-first (hora, timer, status) respondendo sem LLM                                                        | intents respondem com LLM desligada; latência < 1 s                                                                        | `PENDENTE` |
| S4.7 | Gate de voz                                                                                                                   | budgets §4 de `QUALITY.md` medidos e registrados (wake→listening, fala→primeiro áudio); soak 24 h com conversas periódicas | `PENDENTE` |

### S5 — Visão (presença e identidade) — **FASE ADIADA**

_Decisão 2026-07-02:_ câmera fora do escopo v2.0 — o form factor estilo
StackChan não tem cavidade para o módulo (~33×33×17 mm; só a lente passa por
um furo, mas o corpo precisa de espaço interno atrás dele). Slot elétrico
(CS GPIO9, MISO 13), mensagens `SNAPSHOT_*` e capability no HELLO permanecem
reservados. Presença no v2.0 degrada para som (VAD/análise), wake, touch e
sono por inatividade. Rotas de retorno registradas: ArduCam Mega M12 atrás
de janela, ou câmera WiFi independente (ex.: Freenove aposentada) falando
direto com o server.

_Objetivo (quando voltar):_ câmera sob demanda + pipeline semântico na mente.
_Dependências:_ S4.7 + S0.2 + decisão mecânica. _Referência:_ DM4/13.1 do v1.

| ID   | Entrega                                                                                               | Gate de saída                                                                               | Status     |
| ---- | ----------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------- | ---------- |
| S5.1 | `camera_hal` (ArduCam Mega SPI, JPEG sob demanda; driver pelo padrão núcleo/casca sobre o spike S0.2) | captura estável; zero interferência em render/áudio (contadores)                            | `PENDENTE` |
| S5.2 | `SNAPSHOT_*` no NBP/2 (chunks MEDIA, transfer_id, CRC, preempção por CTRL)                            | JPEG íntegro no server < 400 ms p95; controle nunca atrasado por transferência (medido)     | `PENDENTE` |
| S5.3 | Server `VisionMind`: detecção (Haar/DNN) + identificação (Ollama vision) + presença                   | pipeline v1 DM4.9 reproduzido: PRESENT/LEFT_RECENTLY corretos em bancada                    | `PENDENTE` |
| S5.4 | Gaze tracking (rosto→`STIMULUS`+gaze target) + away detection (→ sono)                                | olhos seguem rosto em tempo real; ausência 60 s → sono; retorno → despertar                 | `PENDENTE` |
| S5.5 | Enrollment + ativação de perfil (persona local do v1)                                                 | cadastro pelo dashboard; reconhecimento ativa perfil; tudo local                            | `PENDENTE` |
| S5.6 | Gate de visão                                                                                         | soak 24 h com face loop ativo; recovery limpo ao cobrir/descobrir câmera e reiniciar server | `PENDENTE` |

### S6 — Movimento (servos sob safety)

_Objetivo:_ pescoço expressivo com a disciplina de safety do v1, no
**perfil B** (MG90S provisório — `HARDWARE.md` §Perfis de motion).
_Dependências:_ S3.6. **Bloqueio absoluto:** S6.2+ não inicia sem S6.1
assinado. Trilho B com fuse, INA219 e MOSFET de corte; 5V próprio, GND
comum, nunca o 5V da placa dev.
_Upgrade perfil A (SCS0009 + TTLinker):_ não é fase nova — é troca da casca
do HAL (`servo_hal_scs`, núcleo do protocolo SCS herdado do v1) +
re-execução dos gates S6.2 e S6.5 em bancada. Registrar no painel quando
ocorrer.

| ID   | Entrega                                                                                                                                                                                                                                            | Gate de saída                                                                                                                       | Status     |
| ---- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------- | ---------- |
| S6.1 | **Gate elétrico assinado** (checklist de `ENERGY.md` §4: proteção reversa, fuse por trilho, isolação 5V↔3V3, GND estrela, brownout sob stall)                                                                                                      | checklist em `docs/bringup/` com fotos e medições; assinado antes de qualquer torque no robô                                        | `PENDENTE` |
| S6.2 | `servo_hal` como **interface dupla** (caps `has_feedback`): casca `servo_hal_pwm` (LEDC 17/18) + `motion_safety` com perfis por capability (B: stall via INA219 + corte MOSFET GPIO3; temp coberta por limite de duty) **com host-test do núcleo** | host-tests de veto/fault/idempotência/perfil; em bancada: eixo bloqueado → corte do trilho < 150 ms                                 | `PENDENTE` |
| S6.3 | `motion_service` (interpolação, primitivos de pescoço, heartbeat p/ safety, limites por config NVS, **detach em repouso** — zero PWM/zumbido parado)                                                                                               | movimento suave centro↔limites; posição fora de range vetada (log); silêncio audível em idle                                        | `PENDENTE` |
| S6.4 | Integração expressiva: gaze físico + gestos curtos coordenados com face (conductor mínimo)                                                                                                                                                         | linguagem corporal do v1 reproduzida; retorno limpo ao centro em toda entrada em IDLE (invariante estendida a pescoço)              | `PENDENTE` |
| S6.5 | Gate de movimento                                                                                                                                                                                                                                  | soak 48 h com movimento periódico; injeção de stall/brownout → FAULT correto e recuperação por reset; zero evento de safety perdido | `PENDENTE` |

### S7 — Produto (paridade v1 e release)

_Objetivo:_ fechar paridade com o v1 (exceto visão, adiada) e cortar a
primeira release.
_Dependências:_ S4.7 (S6 pode correr em paralelo; release não bloqueia em S6
se servos atrasarem — produto funciona sem pescoço).

| ID   | Entrega                                                                                                    | Gate de saída                                                                  | Status     |
| ---- | ---------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------ | ---------- |
| S7.1 | Status rail (ícones persistentes: mic, câmera, mente offline, SD, safety) + quick status                   | estados simultâneos alinhados; zero disputa com a face; honesto sob falha      | `PENDENTE` |
| S7.2 | `PersonaMind` + memória longa (SQLite; perfil, preferências, fatos; wipe pelo dashboard)                   | continuidade entre sessões; wipe completo verificado                           | `PENDENTE` |
| S7.3 | Dashboard v2 (React): chat, status, métricas do robô, enrollment, provisioning, wipe                       | fluxos principais utilizáveis; métricas plotadas do `STATUS`                   | `PENDENTE` |
| S7.4 | `SkillHost`: agenda cognitiva sobre `schedule_core`, device commands, busca web opcional                   | timer por voz fim-a-fim; busca degradável sem chave                            | `PENDENTE` |
| S7.5 | **Soak de release: 7 dias** + smoke assinado (`QUALITY.md` §6) + auditoria de segurança (`SECURITY.md` §5) | 7 dias sem crash, heap estável; checklists arquivados em `docs/releases/v2.0/` | `PENDENTE` |
| S7.6 | Release v2.0: tag, OTA assinada publicada, docs revisadas, v1 aposentado formalmente                       | robô do dia a dia rodando v2; repo v1 arquivado como referência                | `PENDENTE` |

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

| Item                                  | Condição de retorno                                                                                                                |
| ------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------- |
| Câmera / visão (fase S5)              | pós-v2.0; **slot reservado** (CS GPIO9, MISO 13, `SNAPSHOT_*` no schema); exige decisão mecânica (cavidade ou câmera WiFi externa) |
| Tela touch                            | pós-v2.0; **pinos já reservados** (ctrl no I2C 4/5, INT no GPIO1)                                                                  |
| IMU MPU-6050                          | pós-v2.0; **pinos já reservados** (I2C 4/5, INT no GPIO8)                                                                          |
| Bateria (charger/boost/fuel gauge)    | pós-v2.0; I2C + revisão do orçamento de energia na entrada                                                                         |
| Touch 3 zonas (MPR121)                | pós-v2.0; I2C, custo zero de GPIO                                                                                                  |
| AEC / full-duplex conversacional      | quando houver meta medida de conversa contínua                                                                                     |
| Wake word customizada                 | quando o fluxo atual justificar treino próprio                                                                                     |
| TLS no firmware                       | não retorna (decisão de arquitetura; ver SECURITY.md)                                                                              |
| HIL com a Waveshare                   | opcional pós-v2.0; nunca dependência                                                                                               |
| Voz bilíngue / RAG / biblioteca local | herdam os planos do v1 depois da paridade                                                                                          |
