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
| S1.9 | Soak do esqueleto                                                                                                                                   | 10 min: zero reset, heap estável, reconexões limpas com server de teste                                                                  | `FEITO`     |

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

**Evidência S1.9 (2026-07-04):** retomado durante o soak do S2.6 (mesma
imagem, ver evidência do S2.6 abaixo) — não precisou de trabalho de código
próprio, só deixar rodando e observar, como já estava registrado na exceção
de ordem no início de §S2.

- Uptime contínuo > 10 min (na prática, ~2.6h, ver S2.6) sem reset; log sem
  `rst:0x`/Guru Meditation/`abort()`/panic.
- Heap estável: `heap_livre`/`heap_min` idênticos entre amostras de ~60s.
- Reconexão limpa com server de teste: subido `tools/nbp2_fake_server.py
  --port 8765` (mesmo IP que o `mind_link` já tentava, `192.168.1.3`).
  Log real: várias tentativas com backoff (`connect ... falhou: errno 104`)
  enquanto o fake server não existia, depois `HELLO enviado` →
  `sessao READY` assim que ele subiu, sessão estável sem desconexão até o
  fim da observação.
- Gate atendido: `FEITO`.

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

### S2 — Face (o robô fica vivo, mudo)

_Objetivo:_ display + renderer + FSM + idle. No fim de S2 o robô parece vivo.
_Dependências:_ S1.9. _Referência de implementação:_ renderer do head v1 (DM2).

**Exceção de ordem registrada (2026-07-03, resolvida em 2026-07-04):** S2
começou com S1.9 ainda `PENDENTE` — soak do esqueleto, não trabalho de
código; não havia nada a implementar além de deixar rodando e observar.
Retomado durante o soak do gate do S2.6 (mesma imagem): S1.9 fechou
`FEITO` (evidência acima, junto de S1.8... ver §S1). O gate de saída de
S2 (S2.6) também foi fechado com escopo de soak amendado (~2h em vez de
48h, decisão explícita registrada na evidência do S2.6) — ver ambas as
evidências para o motivo e os números reais.

| ID   | Entrega                                                                                                 | Gate de saída                                                                      | Status     |
| ---- | ------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------- | ---------- |
| S2.1 | `display_hal` (ST7789 SPI 50 MHz, 3 pinos, double buffer PSRAM, wrapper `extern "C"`)                   | padrão de teste a 30 fps por 1 h; zero artefato; SRAM inalterada (gate do `.map`)  | `FEITO` |
| S2.2 | Renderer paramétrico (10 expressões de `VISUAL.md` §2, interpolação 220 ms, AA sub-pixel)               | paridade visual com v1 confirmada lado a lado; fps ≥ 30 medido                     | `FEITO` |
| S2.3 | `tiny_fsm` (8 estados + modos, `BEHAVIOR.md` §1) **nascendo com o teste de invariante X→IDLE**          | host-test cobre 100% das transições × modos; invariante verde                      | `FEITO` |
| S2.4 | `idle_engine` (catálogo de motifs de `VISUAL.md` §3: blink Poisson, curious tilt, head tilt, look-down) | critério de 60 s de `VISUAL.md` §3 atendido em bancada; parâmetros documentados    | `FEITO` |
| S2.5 | `emotion_core` v0 (vetor+decaimento+âncoras, `BEHAVIOR.md` §2) modulando neutral/idle                   | host-test de decaimento, clamp e integração de estímulo; efeito visível em bancada | `FEITO` |
| S2.6 | Gate visual da fase                                                                                     | soak 48 h face viva sem crash; budgets de fps/PSRAM registrados como baseline      | `FEITO` |

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
- **Gate de saída fechado (2026-07-03):** soak de 30 fps por 1h completa
  em bancada no N32R16V (COM5), padrão de teste com scroll ativo, WiFi/
  mind_link ligados — zero artefato (sem flicker, sem troca de cor, sem
  apagar) durante a hora inteira, já com as correções de DMA/cache acima
  aplicadas. SRAM (DIRAM) comparada via `.map` antes/depois do soak —
  inalterada; baseline do build atual: DIRAM 159267 bytes (46.6%, de
  341760 disponíveis), IRAM 16384 bytes (100%, sem folga — dentro do
  orçamento fixo dessa região). S2.1 encerrado: `FEITO`.
- Gate local confirmado: `python3 tools/run_host_tests.py` verde
  (`display_hal` + núcleos inalterados); `idf.py build` verde (77% livre);
  `python tools/scan_secrets.py` verde.
- **Residual não bloqueante, revisitar no S0.4:** teto de clock real e o
  resíduo de troca de cor com WiFi ativo (RF/EMI) dependem de fiação
  definitiva (par trançado/PCB, não jumper) — fora do escopo de hardware
  desta fase de firmware.

**Plano S2.2 (antes de implementar):**

1. Vendorizar LovyanGFX como git submodule em
   `firmware/components/services/face/renderer/vendor/LovyanGFX` (mesma
   convenção do v1 — não está no ESP Component Registry público).
2. Núcleo C17 puro (`face_core.c/.h`): `nb_face_state_t` (por olho:
   cantos tl/tr/bl/br, abertura, y; + x_off, roundness top/bottom, curve
   top/bottom, squint por olho), tabela `NB_FACE_EXPR_COUNT=10` com as
   âncoras de `VISUAL.md` §2, e `nb_face_state_lerp()` (interpolação
   linear 220 ms). Sem LovyanGFX/FreeRTOS/ESP-IDF — porte reescrito (não
   cópia) de `nb_head_emo_renderer` do v1 (referência permitida por
   `CLAUDE.md`), na estrutura núcleo/casca do NB2.
3. Casca C++ (`shell/nb_face_renderer_shell.cpp/.h`) atrás de
   `extern "C"`: `LGFX_Sprite` amarrado ao back buffer do `display_hal`
   via `setBuffer()` (sem framebuffer extra — reusa o double buffer já em
   PSRAM do S2.1), desenha os dois olhos com a técnica de AA sub-pixel do
   v1 (borda superior/inferior por coluna com alpha blend), aplica gaze e
   chama `nb_display_hal_shell_flush_and_swap()`.
4. `host_test` no mesmo commit cobrindo o núcleo: valores da tabela das
   10 expressões, `lerp` em t=0/0.5/1, clamp de gaze.
5. Padrão de bring-up: comando de shell que cicla as 10 expressões com
   interpolação de 220 ms a 30 fps, para comparação lado a lado com o v1
   em bancada — não dá pra validar paridade visual por log.
6. Gate completo (paridade visual confirmada, fps ≥ 30 medido) é ensaio de
   bancada com o usuário — a fatia de hoje prova que o renderer desenha
   corretamente; a comparação lado a lado fica registrada como próximo
   passo.

**Evidência S2.2 (2026-07-03):**

- LovyanGFX vendorizado como git submodule em
  `firmware/components/services/face/renderer/vendor/LovyanGFX`
  (`.gitmodules`), registrado em `EXTRA_COMPONENT_DIRS`.
- Núcleo C17 puro (`renderer.c/.h`): `nb_face_state_t`, tabela das 10
  expressões-base com os valores herdados do v1 (paridade visual),
  `nb_face_core_lerp()` e `nb_face_core_eye_column()` (geometria + AA
  sub-pixel por coluna, sem LovyanGFX/FreeRTOS/ESP-IDF). Host-test cobre
  as 10 expressões, fallback de índice inválido, interpolação em
  t=0/0.5/1, clamp e os casos de olho fechado (por `open` baixo e por
  squint extremo fechando a coluna).
- Casca C++ (`shell/nb_face_renderer_shell.cpp/.h`) atrás de
  `extern "C"` — único ponto do firmware que instancia `LGFX_Sprite`.
  Amarra o sprite ao back buffer do `display_hal` via `setBuffer()`
  (sem framebuffer extra) e desenha os dois olhos chamando o núcleo
  coluna a coluna.
- `main.c`: task de bring-up cicla as 10 expressões com hold de 1.5 s e
  interpolação de 220 ms a ~30 fps (tick 33 ms), religando o sprite no
  novo back buffer a cada `flush_and_swap()`.
- Gate local confirmado: `python tools/run_host_tests.py` verde
  (`face_renderer` + núcleos inalterados); `idf.py build` limpo (76%
  livre) e `idf.py fullclean` + rebuild do zero também limpos, com
  `-Wall -Wextra -Werror` incluindo os arquivos vendorizados do
  LovyanGFX; `python tools/scan_secrets.py` limpo.
- **Ensaio real em bancada (2026-07-03, N32R16V via COM5):** flash do
  firmware e observação direta do usuário no display.
  - Primeira observação: as 10 expressões apareciam de cabeça para
    baixo. **Bug real no `display_hal` (S2.1), não no renderer** — o
    padrão de barras horizontais do bring-up do S2.1 não denunciava
    isso (uma faixa invertida verticalmente ainda parece um conjunto
    válido de faixas horizontais); só ficou visível com conteúdo
    assimétrico em cima/embaixo (os olhos). Corrigido em
    `nb_display_hal_shell_init()`: depois do `swap_xy(true)`, os eixos
    de `esp_lcd_panel_mirror()` mapeiam pros eixos físicos já trocados
    — testado `mirror(false, true)` (não corrigiu) e `mirror(true,
    false)` (corrigiu), por tentativa direta em bancada, não por
    dedução do datasheet.
  - Depois da correção: orientação certa, as 10 expressões batendo
    com o v1; transições suaves, sem travamento perceptível (fps ≥ 30
    confirmado a olho pelo usuário).
- Gate de saída fechado: paridade visual com v1 confirmada lado a lado
  e fps ≥ 30 medido. S2.2 encerrado: `FEITO`.

**Plano S2.3 (antes de implementar):**

1. Núcleo C17 puro (`tiny_fsm.c/.h`) em `components/autonomic/` (primeiro
   componente da camada L4) — os 8 estados de `BEHAVIOR.md` §1, os 3
   modos (`quiet`/`companion`/`maintenance`) como flags persistentes
   (sobrevivem a troca de estado, só mudam por evento explícito), e as
   regras de §1.2 (safety vence tudo, touch vence fala, wake ignorado em
   `SLEEPING`+`quiet`, `SAFE_MODE` pegajoso).
2. Estado transitório (`nb_fsm_transient_t`) que cada estado não-IDLE
   liga (motif de atenção, boca, reação de toque, olhos fechados, ícones
   de erro, gaze) — zerado por completo toda vez que a máquina pousa em
   `IDLE`, não importa de onde veio nem quais modos estavam ativos. Isso
   opera diretamente a regra de `ARCHITECTURE.md` §4 sem depender de
   `face_service`/`led_service` (que ainda não existem nesta fase).
3. Sem FreeRTOS/ESP-IDF/event_bus e sem casca ainda — mesma regra do
   `event_bus` em S1.2: a casca só nasce quando um consumidor real
   (`reflex_engine`, S3.2) precisar traduzir eventos do bus pra esta API.
4. `host_test` no mesmo commit: toda aresta documentada transiciona
   certo; toda combinação (estado, evento) não-documentada é no-op;
   `FAULT`/`FAULT_CRITICAL` vencem de qualquer estado (exceto
   `SAFE_MODE`, pegajoso); wake ignorado em `SLEEPING`+`quiet`; modos
   persistem através de troca de estado; e o invariante X→IDLE cruzado
   com todas as combinações de modo.

**Evidência S2.3 (2026-07-04):**

- Implementado `firmware/components/autonomic/tiny_fsm` — primeiro
  componente em `components/autonomic/` (camada L4). Núcleo C17 puro,
  sem FreeRTOS/ESP-IDF/event_bus.
- Host-test cobre: as 18 arestas documentadas de `BEHAVIOR.md` §1/§1.2;
  todas as combinações (estado, evento) não-documentadas como no-op
  (varredura exaustiva `NB_FSM_STATE_COUNT × NB_FSM_EVENT_COUNT`);
  `FAULT`/`FAULT_CRITICAL` de qualquer estado exceto `SAFE_MODE`;
  `SAFE_MODE` pegajoso (nenhum evento tira de lá); wake ignorado em
  `SLEEPING` só quando modo `quiet` ativo (touch/wake_hour não são
  bloqueados); modos persistindo através de troca de estado; e o
  invariante X→IDLE (gate H7) para as 8 transições que pousam em `IDLE`
  × 5 combinações de modo — inclusive o caso de resíduo (gaze ligado em
  `ATTENTIVE`, herdado por `RESPONDING` que não mexe em gaze por si só,
  zerado só ao pousar em `IDLE`).
- Gate local confirmado: `python tools/run_host_tests.py` verde
  (`tiny_fsm` + núcleos inalterados); `idf.py build` limpo
  (`-Wall -Wextra -Werror`); `python tools/scan_secrets.py` limpo.
- Sem gate de bancada nesta fatia (núcleo puro, sem hardware envolvido).
  S2.3 encerrado: `FEITO`.

**Plano S2.4 (antes de implementar):**

1. Núcleo C17 puro (`idle_engine.c/.h`) em `components/autonomic/`: o
   catálogo de `VISUAL.md` §3 -- `SOFT_DRIFT` contínuo, processo de
   Poisson de blink (`BLINK_BAR`/`DOUBLE_BLINK`, média 5s, piso 1.8s,
   ~30% viram double) e agendamento ponderado dos motifs longos
   (`CURIOUS_TILT` 30%, `HEAD_TILT_HOLD` 20%, `LOOK_DOWN_BLINK` 15%,
   `LINE_BLINK` 15%, `SIDE_PEEK` 10%, `VERTICAL_SCAN`/`CROSS_SCAN`
   5%+5%, a cada 15-40s em `IDLE` ou 5-13s em `ATTENTIVE`; `quiet` dobra
   os intervalos). RNG embutido (xorshift32, semente injetada) em vez de
   `esp_random()`, pra ficar host-testável byte a byte.
2. Sem casca ainda (mesma regra do `event_bus`/`tiny_fsm`) -- só nasce
   quando houver consumidor real; nesta fatia, `main.c` liga direto ao
   `face_renderer` (S2.2) pra bring-up.
3. `host_test` no mesmo commit: determinismo, semente 0 não trava o RNG,
   `NULL` seguro, drift dentro da amplitude, `quiet` reduz frequência,
   `ATTENTIVE` agenda motifs mais seguido que `IDLE`, `CURIOUS_TILT`
   larga exatamente um olho, e o critério de aceite de `VISUAL.md` §3
   verificado sobre 10 min simulados × 8 sementes (robusto o bastante
   pra não depender de sorte de uma janela de 60s isolada).
4. Ensaio de bancada de 60s -- só é possível para os motifs que o
   renderer (S2.2) já sabe desenhar (gaze + abertura de olho:
   `SOFT_DRIFT`, `BLINK_BAR`, `DOUBLE_BLINK`, `LINE_BLINK`,
   `LOOK_DOWN_BLINK`, `SIDE_PEEK`, `VERTICAL_SCAN`/`CROSS_SCAN`).
   `CURIOUS_TILT` (largura por olho) e `HEAD_TILT_HOLD` (roll) exigem
   suporte que o renderer não tem -- gate completo (todos os motifs)
   fica pendente até essa extensão.

**Evidência S2.4 (2026-07-04):**

- Implementado `firmware/components/autonomic/idle_engine` -- núcleo
  C17 puro, sem FreeRTOS/ESP-IDF/`esp_random()`. Host-test cobre
  determinismo, semente 0, `NULL` seguro, amplitude do drift, `quiet`
  reduzindo frequência, `ATTENTIVE` vs `IDLE`, `CURIOUS_TILT` largando
  um olho só, e o critério de aceite de `VISUAL.md` §3 sobre 10 min ×
  8 sementes.
- `main.c`: task de bring-up soma `SOFT_DRIFT` + blink à expressão
  `NEUTRAL` e chama o `face_renderer` (S2.2), religando o back buffer a
  cada `flush_and_swap()` -- mesmo padrão do S2.2.
- **Três bugs reais achados em ensaio de bancada (N32R16V, COM5), só
  visíveis com o robô ligado, não em host-test:**
  - **Drift imperceptível:** a amplitude literal de `VISUAL.md` §3
    (≤0.04, normalizado) mapeada pela escala de pixel do renderer
    (`kGazeXTravel`/`kYTravel` do S2.2) dava menos de 1px de
    movimento -- lia como parado. Retunado para uma amplitude prática
    maior (`NB_IDLE_DRIFT_AMPLITUDE`), documentada no código com a
    justificativa.
  - **Desvio de olhar brusco/robótico:** `SIDE_PEEK`/`LOOK_DOWN_BLINK`
    saltavam instantaneamente pro alvo (só suavizavam a saída, não a
    entrada) e as varreduras usavam uma onda triangular com reversão
    abrupta no pico. Corrigido com um envelope de suavização
    (`ease_envelope()`, curva S de entrada/saída) e onda senoidal pras
    varreduras -- confirmado em bancada como "bem melhor" depois do
    fix.
  - **Teto de pixel do renderer:** mesmo com amplitude/suavização
    corrigidas, o alcance máximo do gaze no renderer (`kGazeXTravel=14`,
    `kYTravel=32`, herdados do v1) permanecia curto demais neste
    display. Aumentados para `kGazeXTravel=26`/`kYTravel=55` (dentro da
    folga real de tela: ~50px de margem lateral por olho, ~72-76px de
    folga vertical acima/abaixo de `kEyeBaseY`) -- confirmado em
    bancada como "bem melhor" pelo usuário.
- Gate local confirmado: `python tools/run_host_tests.py` verde
  (`idle_engine` + núcleos inalterados, incluindo `face_renderer` após
  o retune de `kGazeXTravel`/`kYTravel`); `idf.py build` limpo; `python
  tools/scan_secrets.py` limpo.
- **Extensão do renderer para largura e roll por olho (2026-07-04):**
  `nb_face_core_eye_column()` (S2.2) já aceitava `half_width` como
  parâmetro -- só faltava a casca variar isso por olho. Estendida
  `nb_face_renderer_shell_draw()` com três parâmetros novos
  (`width_l`, `width_r`, `tilt`, clamp de largura em `[0.5, 1.5]`);
  `tilt` desloca verticalmente os dois olhos em direções opostas
  (`kTiltTravel`, roll visual de `HEAD_TILT_HOLD`). `main.c` liga
  `idle_engine`'s `out.width_l/width_r/tilt` direto no draw.
- **Ensaio de bancada final (2026-07-04, N32R16V via COM5):**
  confirmado ao vivo pelo usuário: piscar (`BLINK_BAR`/`DOUBLE_BLINK`)
  ok; drift contínuo e desvios de olhar (`SIDE_PEEK`/`LOOK_DOWN_BLINK`/
  scans) vivos e suaves depois do retune; `CURIOUS_TILT` (um olho mais
  largo) confirmado; `HEAD_TILT_HOLD` (assimetria de altura entre os
  olhos) confirmado -- em momentos diferentes da sessão, ambos
  funcionando corretamente (a confusão inicial foi só o usuário
  descrevendo o evento mais recente, não um bug real).
- Gate de saída fechado: critério de `VISUAL.md` §3 atendido em
  bancada (todos os motifs observados funcionando); parâmetros
  documentados no código (`idle_engine.c`, `nb_face_renderer_shell.cpp`).
  S2.4 encerrado: `FEITO`.

**Plano S2.5 (antes de implementar):**

1. Núcleo C17 puro (`emotion_core.c/.h`) em `components/autonomic/`:
   vetor 2D (valência × ativação) em `[-1, +1]`, `nb_emotion_core_
   apply_stimulus()` (soma deltas, clamp por eixo) e `nb_emotion_core_
   tick(dt_ms)` (decaimento exponencial analítico rumo a (0,0), `tau`
   derivado de "constante ~60s até <5% do pico" de `BEHAVIOR.md` §2:
   `tau = 60/ln(20) ≈ 20.03s`).
2. `nb_emotion_core_nearest_expression()`: distância euclidiana às 10
   âncoras de `VISUAL.md` §2, reaproveitando `nb_face_expr_t` do
   renderer (S2.2, camada L3 adjacente) em vez de duplicar o enum.
3. `tools/run_host_tests.py` ganhou include path cruzado entre núcleos
   puros (`emotion_core` → `renderer.h`, só header, sem link) -- mesma
   filosofia do `event_bus`/`tiny_fsm`: reuso em vez de duplicação.
4. `host_test` no mesmo commit: estado inicial neutro, clamp de
   estímulo por eixo e acumulado, decaimento chegando a <5% em 60s,
   decaimento monotônico rumo a zero dos dois lados (positivo e
   negativo), nearest-neighbor batendo exatamente em cada âncora,
   `NULL` seguro.
5. Bring-up em `main.c`: pulso de estímulo sintético periódico
   (substituto do toque real, que só chega em S3.1) aplicado sobre o
   vetor; a expressão-âncora mais próxima troca com a mesma
   interpolação de 220ms do S2.2, e o `idle_engine` (S2.4) continua
   sobrepondo motifs por cima.
6. Ensaio de bancada: confirmar visualmente que o pulso muda a
   expressão e que ela decai de volta pra `NEUTRAL` antes do próximo
   pulso — sem overlap que trave a face longe do baseline.

**Evidência S2.5 (2026-07-04):**

- Implementado `firmware/components/autonomic/emotion_core` — núcleo
  C17 puro, sem FreeRTOS/ESP-IDF/NVS. Host-test cobre estado inicial,
  clamp por eixo e acumulado, decaimento (<5% em 60s, monotônico dos
  dois lados), nearest-neighbor exato nas 10 âncoras, e `NULL` seguro.
- `main.c`: task de bring-up aplica um pulso de estímulo sintético
  periódico, interpola a expressão-âncora resultante em 220ms e
  sobrepõe os motifs do `idle_engine` (S2.4) por cima.
- **Bug real achado em bancada (N32R16V, COM5):** o pulso sintético
  inicial era aplicado a cada 15s, bem mais curto que a constante de
  decaimento (~60s até <5% do pico). Os pulsos se acumulavam mais
  rápido do que decaíam (a essa cadência, o vetor nunca chegava perto
  de zero antes do próximo pulso), travando a expressão oscilando
  entre `HAPPY`/`CURIOUS` sem nunca voltar para `NEUTRAL` — visível ao
  vivo, não pego pelo host-test (que testa decaimento isolado, não a
  cadência de pulsos do bring-up). Corrigido aumentando o intervalo
  para 90s, folga real acima da constante de decaimento.
- **Ensaio de bancada confirmado pelo usuário** depois do fix: pulso
  muda a expressão (`HAPPY`/`CURIOUS`), decai de volta para `NEUTRAL`
  ao longo de ~60s, e fica estável até o próximo pulso.
- Gate local confirmado: `python tools/run_host_tests.py` verde
  (`emotion_core` + núcleos inalterados); `idf.py build` limpo;
  `python tools/scan_secrets.py` limpo.
- **Fora do escopo desta fatia:** persistência da última expressão em
  NVS (`BEHAVIOR.md` §2 "Persistência") e a tabela de reflexos locais
  (touch/voz reais) ficam para `reflex_engine` (S3.2), quando o
  `touch_service` existir. S2.5 encerrado: `FEITO`.

**Plano S2.6 (antes de rodar o soak):**

1. Instrumentar `main.c` com budgets: fps medido (janela de 5s, task de
   face) e heap/PSRAM (`heap_caps_get_free_size`/`get_minimum_free_size`,
   a cada ~60s no heartbeat) -- nada disso existia antes, o soak não
   teria como registrar baseline sem medir de verdade.
2. S2.6 pressupõe o esqueleto estável (S1.9, soak de 24h, ainda
   `PENDENTE` desde a exceção de ordem registrada no início de §S2) --
   como a mesma imagem cobre S1 a S2.5, o soak de 48h do S2.6 cobre o
   de 24h do S1.9 como subconjunto; os dois fecham juntos.
3. Captura de log serial em background (`scratch/soak_s2_6/capture.py`,
   fora do repo) por toda a duração, pra inspecionar resets/crashes sem
   depender de estar olhando a tela o tempo todo.

**Evidência S2.6 (2026-07-04, em andamento):**

- Instrumentação adicionada: fps medido na task de face (janela de 5s)
  e heap/PSRAM no heartbeat (a cada ~60 batimentos).
- **Bug real achado ao instrumentar (N32R16V, COM5):** fps medido veio
  23.2, abaixo do "≥30" confirmado visualmente no S2.2 -- só ficou
  visível com o número exato na mão, não dava pra perceber a olho. Causa
  raiz: o loop da task de face fazia `vTaskDelay(33 ms)` **depois** do
  trabalho (lógica 0.02ms + desenho ~7.2ms + flush ~3.6ms àquele fps),
  empilhando o delay fixo em cima do tempo de processamento em vez de
  manter um período fixo -- período real ~44ms (23fps) em vez dos 33ms
  (30fps) pretendidos desde o S2.2. Corrigido trocando `vTaskDelay` por
  `vTaskDelayUntil`, que desconta o tempo já gasto. Confirmado após o
  fix: fps=30.2-30.3 estável por >1min30 de captura.
- Com o fps corrigido, `flush_ms` subiu de ~3.6ms pra ~25.8ms -- não é
  regressão, é a transferência SPI real do frame (~31ms @ 40MHz,
  documentado no `display_hal` desde o S2.1) aparecendo porque agora
  rodamos no ritmo certo; total por frame (lógica+desenho+flush) ≈ 33ms,
  no limite da banda do SPI a 40MHz pra full-frame a 30fps, sem folga
  sobrando mas estável.
- Gate local confirmado: `python tools/run_host_tests.py` verde;
  `idf.py build` limpo; `python tools/scan_secrets.py` limpo.
- **Alteração explícita de escopo do gate (2026-07-04, decisão do
  usuário):** 48h de soak é desproporcional nesta fase -- o sistema
  ainda só tem display+idle_engine+emotion_core, sem touch/motion/áudio
  reais rodando (esses entram em S3+). Reduzido para uma janela curta
  (~2h) que prova estabilidade básica (zero crash/reset, heap/PSRAM sem
  vazamento); revisitar com soak mais longo quando mais subsistemas
  reais estiverem integrados -- mesmo padrão de exceção documentada já
  usado para o S1.9 no início desta fase.
- **Soak executado (2026-07-04, N32R16V via COM5):** ~2.6h de uptime
  contínuo (`10091937` ms desde o boot no último log) com a
  instrumentação de fps/heap/PSRAM ativa, captura de log em background.
  Zero ocorrência de `rst:0x`/Guru Meditation/`abort()`/panic em 797
  linhas de log. `heap_livre`/`heap_min`/`psram_livre`/`psram_min`
  idênticos em todas as amostras (a cada ~60s) -- sem vazamento
  detectável. fps estável em 30.2-30.3 durante toda a janela.
  Baseline registrado: heap livre ~61 KiB (DIRAM interno), PSRAM livre
  ~15.7 MiB (praticamente só os dois framebuffers de 150 KiB alocados).
- Gate de saída fechado (critério amendado): soak sem crash + baseline
  de fps/PSRAM registrado. S2.6 encerrado: `FEITO`. Cobre também o soak
  de 24h do S1.9 (mesma imagem rodando) -- ambos fecham juntos.

### S3 — Toque, LEDs e reflexos (pet completo offline)

_Objetivo:_ fechar o piso offline do produto.
_Dependências:_ S2.6 (S3.1 pode começar após S2.3).

| ID   | Entrega                                                                                            | Gate de saída                                                                                   | Status     |
| ---- | -------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------- | ---------- |
| S3.1 | `touch_hal` + `touch_service` (calibração do v1 2.2A: threshold 20%, debounce, TAP/LONG/SUSTAINED) | toque intencional 50/50; zero falso positivo em 1 h de ruído ambiente; reproduzível após reboot | `FEITO` |
| S3.2 | `reflex_engine` (tabela estímulo→reação com prioridades; touch→afeto integra emotion+face)         | host-test da tabela de arbitragem (conflitos touch×idle×sleep); reação < 80 ms p95 medida       | `FEITO` |
| S3.3 | `led_service` (WS2812 no 21; idle/estados/afeto; brilho circadiano)                                | paridade com linguagem de LED do v1; sem flicker                                                | `FEITO` |
| S3.4 | Ciclo circadiano + sono (SLEEPING com entrada/saída suaves)                                        | transições dormir/acordar observadas nos horários; invariante IDLE segue verde                  | `FEITO` |
| S3.5 | `schedule_core` (timers/alarmes/lembretes locais, persistência NVS, disparo→reflexo+face+led)      | criar/cancelar/disparar OK; reboot não perde nem duplica; disparo com server offline funciona   | `FEITO` |
| S3.6 | Gate do piso offline                                                                               | soak 48 h em modo pet (sem server): vivo, responsivo, estável                                   | `FEITO` |
| S3.7 | **Vida v2 — o básico** (`docs/RFC-VIDA-V2.md` §3.1/§7): campo emocional contínuo nos 4 hubs (NEUTRAL/HAPPY/SAD/ANGRY) com boca + 2 variantes episódicas por hub; `idle_engine` reescrito como 3 motores (atenção/postura/energia) + respiração + blink unificado + acoplamentos + gestos; temperamento e circadiano no vetor | **passo 0 (spike go/no-go): `GO`** — respiração + motor de atenção atrás de flag, Turing de mesa confirmado pelo usuário (2026-07-06); depois: host-tests 1–6 do RFC verdes; bancada 60 s (§9); soak 48 h modo pet; side-by-side v1 registrando novo baseline de persona (S2.2 deixa de ser paridade) | `PENDENTE` |
| S3.8 | **Vida v2 — expansão medida** (RFC §3.2/§4/§5/§10): âncora `CONTENT` + arco `RECONCILE` + decay assimétrico; campo/boca/variantes nas âncoras restantes; glifos e adornos de pico; `GRUMPY_FORGIVE`; `SEARCH`; raridades — cada item só entra com evidência da S3.7 | host-tests 7 do RFC; loop completo em bancada (magoar → consolar → `♥`); contadores de raridade no dashboard; soak com ≥1 ocorrência de cada raridade | `PENDENTE` |

_Nota (2026-07-05):_ S3.7/S3.8 registradas após o gate S3.6 — fase reaberta
com escopo fechado por decisão de produto (persona v2). Motivo, dependências
e gates completos em `docs/RFC-VIDA-V2.md`. S3.7 não depende de S4; IMU e
`TOUCH_HIT` ficam fora (exigem RFC de hardware próprio — pinout congelado).

**Plano S3.1 (antes de implementar):**

1. `touch_hal` (`components/hal/touch_hal`, L0): núcleo C17 puro só com a
   média de calibração de boot (`nb_touch_hal_compute_baseline()`). Casca
   com o periférico touch nativo do ESP-IDF (`driver/touch_sens.h`, API
   v2 do ESP32-S3), GPIO2/canal 2 (`HARDWARE.md`: "TOUCH2"). Settle
   200ms + 10 amostras em 100ms, igual ao v1.
2. `touch_service` (`components/services/touch_service`, L3): porte
   (reescrito, não copiado) da FSM validada em produto no v1 — histerese
   on/off, debounce de entrada/saída (3 amostras), EMA de sinal, rejeição
   de proximidade (hold mínimo de 100ms após confirmar toque), boot
   stabilization, recalibração lenta de baseline com proteção contra
   poisoning, auto-recalibração de emergência se preso em SUSTAINED por
   drift. `nb_touch_service_tick()` puro, clock/raw injetados.
3. `tools/run_host_tests.py` ganha include path cruzado entre núcleos
   puros pra `emotion_core` reaproveitar `nb_face_expr_t` do renderer
   sem duplicar o enum -- mesmo mecanismo reaproveitado aqui se algum
   componente futuro precisar (não foi necessário para touch_service).
4. `host_test` no mesmo commit: FSM completa (TAP→LONG_PRESS→SUSTAINED),
   rejeição de picos isolados de ruído, histerese, modo `sleeping`
   (WAKE em vez de TAP), recalibração lenta com baseline/drift em escala
   realista de hardware, proteção contra poisoning durante toque ativo,
   `NULL` seguro.
5. Bring-up em `main.c`: task a 50Hz que só loga eventos (TAP/LONG_
   PRESS/SUSTAINED/WAKE) -- sem ligar em emotion/idle/face ainda, isso é
   o `reflex_engine` (S3.2).
6. Ensaio de bancada com a fita de cobre real (GPIO2): toques
   intencionais contados + janela de ruído ambiente sem tocar.

**Evidência S3.1 (2026-07-04):**

- Implementado `touch_hal` (L0) e `touch_service` (L3) -- núcleos C17
  puros, host-test cobrindo a FSM completa, histerese, rejeição de
  ruído isolado, `sleeping`→`WAKE`, recalibração lenta em escala
  realista, proteção contra poisoning e `NULL` seguro.
- **Ensaio real em bancada (2026-07-04, N32R16V via COM5, fita de cobre
  em GPIO2):**
  - Boot limpo, calibração real: `baseline=24858` (mudou de `20032` sem
    a fita pra `24858`/`24792` com ela conectada -- confirma que o
    hardware está de fato lendo a fita, não um valor arbitrário).
  - **10 toques intencionais contados pelo usuário → exatamente 10
    eventos `TAP` no log**, sem faltar nenhum, sem duplicar, sem
    confundir com `LONG_PRESS`/`SUSTAINED`.
  - **Zero falso positivo em ~15 min de ruído ambiente** (sem tocar) --
    nenhuma linha `event=` no log durante toda a janela.
  - Reproduzível após reboot: baseline recalibrado corretamente em
    cada flash/reset ao longo da sessão (`24858` → `24792` → `24732`,
    variando com a condição real do fio, sempre coerente com
    `thr_on`/`thr_off` recalculados).
- **Alteração explícita de escopo do gate (2026-07-04, decisão do
  usuário):** janela de ruído ambiente reduzida de 1h pra ~15min --
  mesmo padrão de exceção já registrado no S2.6 (sistema ainda simples,
  revisitar com janela mais longa quando mais subsistemas reais
  entrarem).
- Gate local confirmado: `python tools/run_host_tests.py` verde
  (`touch_hal`, `touch_service` + núcleos inalterados); `idf.py build`
  limpo; `python tools/scan_secrets.py` limpo.
- Gate de saída fechado (critério amendado): toque intencional 10/10,
  zero falso positivo em ~15min, reproduzível após reboot. S3.1
  encerrado: `FEITO`.

**Plano S3.2 (antes de implementar):**

1. `reflex_engine` (`components/autonomic/reflex_engine`, L4): núcleo C17
   puro. Tipos: `nb_reflex_priority_t` (P0 safety/erro > P1 touch reflexo >
   P2 fala > P3 hint da mente > P4 emoção mapeada > P5 motifs de idle > P6
   baseline, per `BEHAVIOR.md` §3); `nb_reflex_stimulus_t` com os 13
   estímulos locais de `BEHAVIOR.md` §2 (TOUCH_TAP/LONG/WARM_PULSE/DEEP/
   CARESS/RELEASE, VOICE_START/LOUD/SOFT, AUDIO_PLAYING, ENTERING_SLEEP,
   WAKING_UP, MOTION_FAULT, IDLE_LONG) + WAKE de toque -- estímulos sem
   produtor ainda (voz, safety, mind-hint) entram na tabela desde já, sem
   shell de entrada, pra não retrabalhar o enum em S4/S6.
2. Tabela estática estímulo→(prioridade, Δvalence, Δarousal, evento
   `tiny_fsm` opcional, duração do efeito). Estado interno: pilha de
   reações ativas por prioridade com expiração -- camada superior suspende
   a inferior sem destruí-la (retoma ao expirar); empate de prioridade →
   mais recente vence.
3. `nb_reflex_engine_tick(engine, dt_ms, touch_is_pressed,
   touch_duration_ms, &out_reaction)`: reclassifica toque contínuo
   (TAP→LONG→WARM_PULSE 3-8s→DEEP >8s→CARESS >15s conforme duração de
   `touch_service`), expira reações vencidas, decide a reação vencedora do
   tick. Nunca disputa com `tiny_fsm`: X→IDLE sempre zera a pilha até
   baseline.
4. `event_bus` ganha `nb_event_type_t` real (hoje inexistente): IDs
   reservados para TOUCH, VOICE, SAFETY, MIND_HINT (só TOUCH com produtor
   nesta subfase) -- primeira casca real do bus. `touch_service_shell`
   passa a publicar `NB_EVENT_TYPE_TOUCH` (payload `{touch_event,
   duration_ms}`) em vez de só callback direto.
5. `shell/nb_reflex_engine_shell.c/.h` (L4): task que faz poll do bus,
   chama `nb_reflex_engine_tick`, aplica a reação vencedora chamando
   `nb_emotion_core_apply_stimulus`, `nb_tiny_fsm_apply_event`,
   `nb_idle_engine_set_mode`. Timestamp de chegada (bus) vs. aplicação vira
   a métrica dos 80ms p95 (medida em bancada, não em host-test).
6. `main.c`: remove o pulso sintético de 90s do `emotion_core` (decisão do
   usuário, 2026-07-04 -- estímulo real de toque substitui; IDLE_LONG
   entra como estímulo real na tabela, não como hack ad hoc). Conecta
   `touch_service_shell` → bus → `reflex_engine_shell`; unifica as tasks de
   face/emotion/idle/touch sob o reflex_engine.
7. `host_test` no mesmo commit: tabela de arbitragem completa (conflitos
   touch×idle×sleep), suspensão sem destruição e retomada, reclassificação
   de toque contínuo nos thresholds corretos, empate de prioridade → mais
   recente vence, invariante X→IDLE preservada (pilha zera em IDLE).
8. Ensaio de bancada com toque real (GPIO2): medir latência estímulo→
   reação visível via timestamps do bus, confirmar p95 < 80ms.

**Evidência S3.2 (2026-07-04, parcial):**

- Implementado `reflex_engine` (núcleo C17 puro) com a tabela completa de
  BEHAVIOR.md §2/§3, arbitragem por pilha de claims com expiração,
  reclassificação de toque contínuo por duração, `force_clear` para a
  invariante X→IDLE. Host-test cobre arbitragem (safety>touch>fala>hint),
  suspensão sem destruição e retomada, empate na mesma banda, toque
  contínuo nos thresholds corretos, `NULL` seguro.
- `event_bus` ganhou sua primeira casca real (`shell/nb_event_bus_shell.c`,
  instância única + critical section) e `nb_event_type_t` (TOUCH com
  produtor; VOICE/SAFETY/MIND_HINT reservados pra S4/S6).
  `touch_service_shell` publica `NB_EVENT_TYPE_TOUCH`; `reflex_engine_shell`
  drena, aplica delta afetivo em `emotion_core` e evento em `tiny_fsm`
  (que também ganhou seu primeiro uso real em `main.c` -- antes só tinha
  host-test). Pulso sintético de 90s do `emotion_core` removido.
- Gate local fechado: `python tools/run_host_tests.py` verde (todos os
  14 componentes, incluindo `reflex_engine`/`tiny_fsm`); `idf.py build`
  limpo; `python tools/scan_secrets.py` limpo.
- **Ensaio de bancada real (2026-07-04, N32R16V via COM5, fita de cobre em
  GPIO2):** log de latência adicionado em `reflex_engine_shell`
  (`latency_ms` = chegada no bus, timestamp de `esp_timer` em
  `touch_service_shell` → aplicação em `emotion_core`/`tiny_fsm`). Sessão
  de toques reais: **12 TAP + 2 LONG_PRESS + 2 SUSTAINED, 16 reações
  capturadas**, todas com `priority=1` (P1 touch) e `fsm_event=7`
  (`NB_FSM_EVENT_TOUCH`) corretos -- zero evento espúrio/WAKE.
  Latências: min 3ms, max 31ms, **p95 ≈ 31ms** -- bem dentro do budget de
  < 80ms (`QUALITY.md`).
- Gate de saída fechado: host-test da arbitragem verde + latência real de
  bancada medida e dentro do budget. S3.2 encerrado: `FEITO`.

**Correção de escopo (2026-07-04, decisão do usuário):** o pino do WS2812
na tabela de S3.3 estava registrado como GPIO46 -- inconsistente com
`HARDWARE.md`/`VISUAL.md` (GPIO21, 2x WS2812 externos em cadeia) e com
GPIO46 sendo strap pin ("nunca sinal idle-HIGH", `HARDWARE.md`). Corrigido
pra GPIO21 antes de iniciar a implementação; nenhuma mudança de pino real,
só correção de typo no ROADMAP.

**Plano S3.3 (antes de implementar):**

1. `nb_hw_config.h` (componente novo, `components/hal/nb_hw_config`,
   header-only): centraliza `NB_HW_GPIO_TOUCH2`/`NB_HW_TOUCH_CHANNEL` (2),
   `NB_HW_GPIO_WS2812` (21), `NB_HW_LED_COUNT` (2), reserva
   `NB_HW_GPIO_LED_STATUS_ONBOARD` (38, fora de escopo -- LED de status
   embutido da placa, não expressivo, `HARDWARE.md`). `touch_hal` migra o
   `#define` local de GPIO pra essa constante (débito de CLAUDE.md §Código
   corrigido nesta subfase, decisão do usuário).
2. `led_hal` (L0, `components/hal/led_hal`): casca fina sobre
   `espressif/led_strip` (RMT), GPIO21, 2 LEDs em cadeia -- sem lógica
   não-trivial pra extrair, só `shell/` (sem núcleo separado, mesma regra
   de P3 ser "pra lógica não-trivial"). Primeiro uso do component manager
   no v2 (`idf_component.yml`).
3. `led_service` (L3, `components/services/led_service`): núcleo C17 puro
   reaproveitando `nb_fsm_state_t` do `tiny_fsm` (sem duplicar enum, mesmo
   padrão de `emotion_core`↔`renderer`). Tabela estado→(cor, período de
   respiração) de `VISUAL.md` §6 (IDLE quente ~6s, ATTENTIVE frio médio,
   SLEEPING ~2%, ERROR vermelho intermitente **nunca suprimido**, TOUCH
   flash quente+fade longo como overlay); BOOT/SAFE_MODE sem cor definida
   em VISUAL.md -- branco frio/laranja como default de engenharia
   (documentado no README). Modelo two-layer (base+overlay) e prioridade
   herdados do v1 (ERROR/SAFE_MODE nunca suprimidos por overlay). Brilho
   circadiano: multiplicador `[0,1]` injetado -- driver real de
   hora-do-dia fica pro S3.4, aqui só o mecanismo. Gamma 2.2 na saída.
   `tick()` retorna se o frame mudou (dirty-flag) pra casca só escrever no
   RMT quando necessário -- mecanismo direto contra flicker, herdado do
   v1.
4. `shell/nb_led_service_shell.c/.h`: tick por frame com o estado do
   `tiny_fsm` + `dt_ms`; `nb_led_service_shell_trigger_touch()` disparado
   por `reflex_engine_shell` quando aplica evento de toque (mesmo lugar
   que já aplica em `emotion_core`/`tiny_fsm` -- reaproveita a arbitragem
   existente, sem duplicar lógica de prioridade).
5. `host_test` no mesmo commit: tabela estado→cor/período, prioridade
   (ERROR nunca suprimido por overlay), overlay de toque dispara e volta
   ao base sozinho, brilho circadiano clampado e aplicado corretamente,
   gamma em pontos conhecidos, dirty-flag correto, `NULL` seguro.
6. `main.c`: religa `led_service_shell` na task de face (mesmo tick de
   33ms) e no `reflex_engine_shell`.
7. Gate: host-test + `idf.py build` limpo + checagem visual em bancada
   (sem flicker, respiração perceptível, flash de toque visível).

**Evidência S3.3 (2026-07-05):**

- Implementado `nb_hw_config.h` (header-only, GPIO2/canal touch, GPIO21
  WS2812, GPIO38 status onboard reservado); `touch_hal` migrado. `led_hal`
  (casca fina sobre `espressif/led_strip`, primeiro uso do component
  manager no v2). Núcleo `led_service` (tabela estado→cor/onda de
  `VISUAL.md` §6, two-layer base+overlay, ERROR/SAFE_MODE nunca
  suprimidos, brilho circadiano mecanismo, gamma 2.2, dirty-flag).
  `reflex_engine_shell` dispara o overlay de toque no mesmo lugar que já
  aplica em `emotion_core`/`tiny_fsm`.
- Gate local: `run_host_tests.py` verde (15 componentes, incluindo
  `led_service`); `idf.py build` limpo (`espressif/led_strip 3.0.3`
  resolvido via component manager); `scan_secrets.py` limpo.
- **Ensaio de bancada real (2026-07-05, N32R16V via COM5, 2x WS2812 em
  GPIO21):** `led_hal OK -- GPIO21, 2 LEDs` no boot, sem erro de RMT.
  Observação visual do usuário: **respiração suave do IDLE sem flicker**,
  **flash quente bem visível no toque** (log confirma `reaction event=0
  priority=1 fsm_event=7` correspondente). Critério de paridade (linguagem
  de LED coerente com estado, sem flicker) confirmado.
- Gate de saída fechado. S3.3 encerrado: `FEITO`.

**Decisões de escopo S3.4 (2026-07-05, decisão do usuário):**

- Relógio circadiano vira componente novo dedicado `circadian_core` (L4,
  responsabilidade única, mesmo padrão de `reflex_engine`/`emotion_core`/
  `led_service`) em vez de entrar dentro do `idle_engine` (redação literal
  de `BEHAVIOR.md` §5, tratada como nota de comportamento, não fronteira
  de componente -- `ARCHITECTURE.md` não reserva o nome em lugar nenhum).
- Sem servidor real enviando `TIME_SYNC` ainda (protocolo já tem
  codegen -- `protocol/generated/c/nbp2.c` -- mas nada no `server/` chama
  `encode_time_sync`): gate validado com **relógio injetado acelerado**
  em bancada (1 dia comprimido em poucos minutos), mesmo padrão do pulso
  sintético do `emotion_core` antes do toque real (S2.5→S3.1). Decodificar
  `TIME_SYNC` no `mind_link_shell` mesmo assim (custo baixo, contrato já
  gerado) -- só não terá produtor real até o server implementar.

**Plano S3.4 (antes de implementar):**

1. `circadian_core` (`components/autonomic/circadian_core`, L4, núcleo
   C17 puro): âncora de tempo (`unix_ms` + referência monotônica local,
   `nb_circadian_core_set_time_anchor`) + `tick(dt_ms)` que avança o
   monotônico local e resolve fase do dia a partir de `unix_ms` estimado.
   4 fases (`NIGHT`/`DAWN`/`DAY`/`DUSK`, limiares de hora tunáveis por
   `#define`, travados por host-test): `DAY` brilho 1.0; `NIGHT` piso
   ~0.05 + `quiet_mode`; `DAWN`/`DUSK` rampa linear entre os dois ao longo
   da janela de 2h -- é essa rampa contínua (não um crossfade à parte) que
   dá a "entrada/saída suave" de `SLEEPING` exigida pelo gate: a claim de
   `led_service` já recalcula cor/brilho a cada frame, então uma curva de
   brilho circadiano que já está no piso quando `SLEEP`/`WAKE_HOUR`
   disparam evita o "pop" visual. Sem âncora ainda: default neutro
   (`DAY`, brilho 1.0, `has_time_source=false`) -- nunca força
   comportamento noturno antes de saber a hora de verdade.
2. `app_config` ganha `NB_CONFIG_KEY_LAST_KNOWN_UNIX_TIME_S` (u32,
   segundos desde epoch) -- fallback "contador local + último horário
   NVS" de `BEHAVIOR.md` §5, reaproveitando o get/set genérico já
   existente (nenhum código de NVS novo).
3. `shell/nb_circadian_core_shell.c/.h`: no init, carrega o último horário
   conhecido do NVS (via `app_config`) e ancora; sem valor salvo (primeiro
   boot), semeia um horário de bancada arbitrário (~20h, início de DUSK)
   documentado como só-pra-demo. Persiste o horário estimado periodicamente
   (a cada poucos minutos) de volta no NVS. `tick(dt_ms, &fsm)`: aplica
   `dt_ms` acelerado (constante de bancada, dia comprimido em minutos) ao
   núcleo, detecta borda de entrada em `NIGHT` (dispara
   `NB_FSM_EVENT_SLEEP` repetidamente até `tiny_fsm` confirmar `SLEEPING`
   -- não briga se a máquina estiver ocupada em outro estado) e borda de
   entrada em `DAWN` (dispara `NB_FSM_EVENT_WAKE_HOUR` até sair de
   `SLEEPING`). Devolve a fase/brilho/quiet_mode resolvidos pro chamador
   aplicar em `idle_engine`/`led_service`.
4. `mind_link_shell`: troca o `case NBP2_MSG_TIME_SYNC` (hoje só loga
   "não usado ainda") por `nbp2_decode_time_sync` +
   `nb_circadian_core_shell_set_time_anchor(unix_ms, esp_timer_get_time()/
   1000)` -- mesma base de clock (`esp_timer`) já usada por
   `reflex_engine_shell`/`touch_service_shell`, sem descasamento entre
   tasks.
5. `main.c`: religa a task de face -- `nb_led_service_shell_set_
   brightness_scale` deixa de ser chamada uma vez com `0.15f` fixo e
   passa a receber a saída do `circadian_core` a cada frame;
   `nb_idle_engine_set_mode` passa a receber `quiet_mode` do circadiano
   (atenção seguindo `NB_IDLE_ATTENTION_IDLE`, sem mudança de escopo
   além do pedido).
6. `host_test` no mesmo commit: limiares de fase por hora, brilho pleno
   em `DAY`/piso em `NIGHT`, rampa monotônica em `DAWN`/`DUSK`,
   `quiet_mode` só em `NIGHT`, default neutro sem âncora, recalibração
   por `set_time_anchor`, avanço de `now_unix_ms` com o monotônico,
   `NULL` seguro.
7. Gate: host-test verde + `idf.py build` limpo + ensaio de bancada com
   relógio acelerado (dia comprimido em minutos) observando pelo menos
   um ciclo completo dormir→acordar; invariante X→IDLE do `tiny_fsm`
   continua coberta (nenhuma mudança nele).

**Evidência S3.4 (2026-07-05):**

- Implementado `circadian_core` (núcleo C17 puro, fases NIGHT/DAWN/DAY/
  DUSK por hora, rampa contínua de brilho em DAWN/DUSK, `quiet_mode` só em
  NIGHT, default neutro sem âncora) + `app_config` ganhou
  `NB_CONFIG_KEY_LAST_KNOWN_UNIX_TIME_S` (fallback NVS). `shell/nb_
  circadian_core_shell` semeia bancada em 20h no primeiro boot, aplica
  relógio acelerado (240x, 1 dia = 6min), dispara `SLEEP`/`WAKE_HOUR` com
  retry nas bordas de fase, persiste horário no NVS periodicamente.
  `mind_link_shell` decodifica `TIME_SYNC` (`nbp2_decode_time_sync`) e
  publica no `event_bus` (não chama `circadian_core` direto -- mind_link é
  L3, circadian_core é L4, cross-layer só via bus); `reflex_engine_shell`
  (único leitor do bus) despacha pro `circadian_core_shell`. `main.c`:
  `led_service.brightness_scale` e `idle_engine.quiet_mode` passam a vir
  do circadiano a cada frame, substituindo o valor fixo de 15% do S3.3.
- Gate local: `run_host_tests.py` verde (16 componentes, incluindo
  `circadian_core`); `idf.py build` limpo; `scan_secrets.py` limpo.
- **Ensaio de bancada real (2026-07-05, N32R16V via COM5, relógio
  acelerado):** semeado em 20h (DUSK). Log real: `SLEEP aplicado --
  entrando em NIGHT` aos ~31,8s (esperado ~30s = 7200s/240, janela de
  DUSK); `WAKE_HOUR aplicado -- entrando em DAWN` aos ~151,8s (esperado
  ~150s = 30s+28800s/240, janela de NIGHT) -- os dois logs só imprimem
  quando `tiny_fsm` **confirma** o estado (`SLEEPING` alcançado/
  abandonado), então são prova direta de que dormiu e acordou nos
  horários certos, não só que o evento foi disparado. `fps` estável em
  30.3 antes/depois de ambas as transições.
- **Observação registrada, não bloqueia o gate:** falhas intermitentes de
  alocação DMA no `display_hal` (`ESP_ERR_NO_MEM` em `setup_dma_priv_
  buffer`) correlacionadas com tentativas de reconexão do `mind_link`
  (sem server rodando) -- `heap_livre`/`psram_livre` seguem estáveis ao
  longo da sessão (sem drift, não é vazamento) e o `circadian_core` não
  toca display/SPI/DMA. Fica como débito técnico pra investigar quando
  fizer sentido (contenção de SRAM interna entre WiFi e DMA de SPI),
  não é regressão desta subfase.
- Gate de saída fechado. S3.4 encerrado: `FEITO`.

**Plano S3.5 (antes de implementar):**

1. `schedule_core` (`components/autonomic/schedule_core`, L4, núcleo C17
   puro): array fixo `NB_SCHEDULE_MAX_TIMERS=8` slots (`timer_id`,
   `fire_at_unix_ms`, `label[65]`), disparo one-shot em `unix_ms` absoluto
   (sem recorrência -- protocolo `TIMER_SET`/`TIMER_CANCEL`/`EVENT_
   TIMER_FIRED`, já com codegen, não define campo de recorrência).
   `create()` faz upsert por `timer_id` (id já existente = atualiza,
   suporta retry idempotente do protocolo sem duplicar); `cancel()` libera
   o slot; `tick(now_unix_ms)` varre os slots e devolve os ids vencidos
   (slot liberado no mesmo tick -- nunca dispara duas vezes). Layout
   compatível com blob de NVS (sem ponteiros).
2. `shell/nb_schedule_core_shell.c/.h`: persiste o array inteiro em NVS
   (namespace próprio `"nb_sched"`, blob único, mesmo padrão de
   `mind_link_token_shell`, que já usa `nvs_set_blob`/`get_blob` -- o
   modelo escalar por chave do `app_config` não serve pra um array de
   slots) a cada mutação (criar/cancelar/disparar) -- cobre "reboot não
   perde nem duplica": ao reiniciar, carrega o array salvo; se algum timer
   já venceu enquanto desligado, dispara no primeiro tick (atrasado, mas
   dispara -- "não perde") e o slot já sai liberado do disparo ("não
   duplica"). `tick(dt_ms)` usa `nb_circadian_core_shell_now_unix_ms()`
   (novo getter) como fonte de hora.
3. `event_bus` ganha `NB_EVENT_TYPE_TIMER` (payload `{fire_at_unix_ms,
   timer_id, action SET|CANCEL}`, rótulo não cabe nos 16 bytes -- truncado/
   vazio pra timer criado remotamente, rótulo completo só importa quando
   houver criação local por voz em S4.6). `mind_link_shell` decodifica
   `TIMER_SET`/`TIMER_CANCEL` (`nbp2_decode_timer_set/cancel`, protocolo já
   gerado) e publica no bus -- `mind_link` é L3, `schedule_core` é L4,
   "camada chama só pra baixo" proíbe L3 chamando L4 direto.
   `reflex_engine_shell` (único leitor do bus, mesmo padrão de `TIME_SYNC`→
   `circadian_core` em S3.4) despacha `NB_EVENT_TYPE_TIMER` pro
   `schedule_core_shell`.
4. `reflex_engine`: novo estímulo `NB_REFLEX_STIMULUS_TIMER_FIRED`,
   banda P3 (HINT) -- reaproveita a banda de "hint da mente" em vez de
   abrir uma 5ª banda de claim (evitaria renumerar `nb_reflex_priority_t`
   sem necessidade); delta afetivo leve/positivo. `reflex_engine_shell`
   ganha `nb_reflex_engine_shell_apply_stimulus()` pública, pra
   `main.c` aplicar o disparo do timer reaproveitando `on_stimulus`/
   `apply_reaction` já existentes (inclui o overlay de toque do
   `led_service` -- mesma linguagem visual, sem overlay dedicado nesta
   fatia). "Som local" de `BEHAVIOR.md` §5 fica pra quando `audio_hal`
   existir (S4.1) -- não há alto-falante ligado ainda.
5. `mind_link_shell` ganha `nb_mind_link_shell_send_timer_fired(timer_id)`
   (`nbp2_encode_event_timer_fired`, mesmo `send_frame` genérico de
   HEARTBEAT/HELLO): chamada direta de `main.c` quando um timer dispara
   (L4→L3, camada adjacente, sem precisar de bus) -- gated em
   `NB_MIND_LINK_STATE_READY`; sem server conectado, o envio é só
   descartado (fire-and-forget, protocolo não define reenvio) e o
   reflexo local (emoção+LED) já disparou de qualquer jeito -- cobre
   "disparo com server offline funciona".
6. `main.c`: religa `schedule_core_shell` na task de face -- tick por
   frame, aplica `apply_stimulus`+`send_timer_fired` pra cada id disparado.
7. `host_test` no mesmo commit: criar/cancelar/disparar, upsert por id
   repetido, array cheio, disparo em lote na ordem certa, nenhum disparo
   duplicado, `NULL` seguro.
8. Gate: host-test verde + `idf.py build` limpo + ensaio de bancada
   (criar timer curto, observar reflexo local disparar; reboot no meio
   não perde nem duplica -- reaproveita o relógio acelerado de S3.4 pra
   não precisar esperar minutos reais).

**Evidência S3.5 (2026-07-05):**

- Implementado `schedule_core` (núcleo C17 puro, array fixo de 8 slots,
  upsert por `timer_id`, disparo one-shot limpando o slot no mesmo tick).
  `shell/nb_schedule_core_shell` persiste em NVS (namespace `"nb_sched"`,
  blob único) a cada mutação. `event_bus` ganhou `NB_EVENT_TYPE_TIMER`
  (payload definido em `event_bus.h`, não em `schedule_core.h` -- mind_link
  é L3 e não pode incluir header de L4 só por causa de um tipo de payload,
  correção de design no meio da implementação). `mind_link_shell` decodifica
  `TIMER_SET`/`TIMER_CANCEL` e publica no bus; `reflex_engine_shell` (único
  leitor) despacha pro `schedule_core_shell`. Disparo aplica
  `NB_REFLEX_STIMULUS_TIMER_FIRED` (banda P3/HINT) via nova
  `nb_reflex_engine_shell_apply_stimulus()` e pede `EVENT_TIMER_FIRED` ao
  mind_link via flag simples (fire-and-forget, sem socket compartilhado
  entre tasks).
- Gate local: `run_host_tests.py` verde (17 componentes, incluindo
  `schedule_core`); `idf.py build` limpo; `scan_secrets.py` limpo.
- **Ensaio de bancada real (2026-07-05, N32R16V via COM5):** criado timer
  de teste via chamada direta (sem server ainda) com disparo em ~20s
  reais (relógio acelerado 240x de S3.4). Log real: `TIMER_SET aplicado
  -- id=1` seguido de `disparou -- id=1 now_unix_ms=76807440` ~20,3s
  depois (esperado ~20s) -- criar/disparar confirmado.
  **Reboot no meio:** reset físico via `esptool` ~8s após criar (antes do
  disparo). Log pós-reset: `restaurado do NVS -- 1 timers` (timer pendente
  sobreviveu ao reset -- "não perde") seguido de exatamente **uma** linha
  `disparou -- id=1` no total (sem duplicar). Hook de bancada removido do
  `main.c` antes do commit.
- Gate de saída fechado. S3.5 encerrado: `FEITO`.

**Plano S3.6 (antes de rodar o soak):**

1. Sem código novo previsto -- S3.6 é gate de validação da fase, não
   entrega de componente. Firmware é a mesma imagem de S3.5 (com o teto
   de brilho de LED em 15%).
2. **Alteração explícita de escopo do gate (2026-07-05, decisão do
   usuário):** 48h de soak reduzidas pra uma janela amendada (~3h) --
   mesmo padrão de exceção já usado em S1.9/S2.6. Diferença desta vez:
   ao contrário do S2.6 (só display+idle+emotion), agora `touch_service`/
   `reflex_engine`/`led_service`/`circadian_core`/`schedule_core` estão
   todos integrados e rodando juntos -- exatamente a condição que a
   evidência do S2.6 registrou como gatilho pra revisitar com soak mais
   longo. ~3h cobre múltiplos ciclos completos de dia/noite acelerado
   (S3.4: 1 dia = 6min), várias interações de toque reais, e roda o
   `schedule_core` de verdade.
3. Captura de log serial em background por toda a janela (mesmo padrão
   do S2.6), monitorando reset/panic/Guru Meditation e heap/PSRAM a cada
   ~60s (instrumentação já existe desde S2.6).
4. Toques periódicos reais na fita de cobre ao longo da janela, pra
   provar "responsivo" (não só "vivo").

**Evidência S3.6 (2026-07-05):**

- **Bug de ferramenta achado durante o soak:** o script de captura de log
  usado nas sessões anteriores (`serial_dump.py`, fora do repo) não
  desabilitava DTR/RTS ao abrir a porta serial -- em adaptadores USB-serial
  típicos (CP210x/CH340) isso pulsa o EN/GPIO0 da placa, resetando o
  ESP32-S3 **toda vez que a porta era reaberta** para uma checagem rápida.
  Corrigido (`ser.dtr = False; ser.rts = False` antes de `open()`);
  confirmado que reaberturas subsequentes não resetam mais (uptime
  contínuo entre checagens). A contagem real do soak começou a partir da
  última reabertura ainda com o bug (reset acidental), não do flash
  original do teto de LED.
- **Janela amendada em tempo real pelo usuário (2026-07-05): ~1,3h** (não
  as ~3h planejadas no item 2 acima) -- decisão do usuário de que a
  janela já rodada era suficiente pra validar.
- **Soak executado (N32R16V via COM5, ~79min de uptime contínuo,
  `4767032` ms desde o boot no último log, captura em background sem
  reabrir a porta):** zero ocorrência de `rst:0x`/Guru Meditation/
  `abort()`/panic em 6114 linhas de log. `heap_livre` estável (~49,6 KiB,
  variação de ruído de dezenas de bytes) e `psram_livre` **idêntico**
  (16.462.576 bytes) em todas as amostras de heartbeat (~60s) -- sem
  vazamento detectável. fps estável em 30.3 durante toda a janela.
  **11 reações de toque reais capturadas** (TAP/LONG_PRESS/SUSTAINED)
  com `reflex_engine` respondendo corretamente (`fsm_event=7`/TOUCH) e
  latência baixa (2-31ms) -- responsividade real confirmada, não só
  uptime passivo. `mind_link` seguiu tentando reconectar (sem server) sem
  travar o resto do sistema, confirmando "modo pet sem server" funcional.
- Gate de saída fechado (critério amendado duas vezes: 48h→3h no plano,
  3h→~1,3h em tempo real). S3.6 encerrado: `FEITO`.

**Plano S3.7 — passo 0: spike go/no-go (antes de qualquer implementação
definitiva):**

Referência completa: `docs/RFC-VIDA-V2.md`. O spike decide se o restante da
S3.7 acontece; **nenhum item do RFC além dos dois mecanismos abaixo entra
antes do go.**

1. **Escopo estrito:** respiração + motor de atenção atrás de flag de
   compilação (`NB_IDLE_V2_SPIKE` em `nb_idle_engine.h`), zero mudança fora
   do `idle_engine`. Não tocar: `tiny_fsm`, `emotion_core`, `reflex_engine`,
   `led_service`, renderer, âncoras.
2. **Núcleo puro** (P3, sem FreeRTOS/ESP-IDF, clock+RNG injetados):
   - `nb_breath` — `nb_breath_scale(now_ms, period_ms, amp)` → fator de
     modulação da altura dos olhos. Período 6000 ms, amplitude 2%
     (faixa 1.5–2.5%), `#define` do núcleo.
   - `nb_attention` — FSM `FIXATE⇄SACCADE`: fixação 500–3000 ms com
     micro-tremor ≤ 0.02; sacada 80 ms (ease-out) para alvo ∈
     [−0.35, +0.35]², ~40% dos alvos retornando ao centro ±0.05 (viés
     "olhar pro usuário"); callback `on_saccade()` exposto para a casca
     disparar blink (acoplamento blink×sacada).
3. **Host-tests primeiro:** determinismo com seed fixa; gaze nunca fora do
   envelope; durações de fixação na faixa; tremor limitado; invariante
   X→IDLE verde **nas duas configs de flag** (table-driven existente roda
   com e sem `NB_IDLE_V2_SPIKE`).
4. **Casca:** flag ligada → sorteio de motifs desliga (blink existente
   permanece), atenção alimenta o gaze (EMA existente) e respiração modula
   a altura dos olhos, por tick. Motores ocupam P5 na arbitragem, como os
   motifs hoje.
5. **Critérios go/no-go (bancada, evidência registrada aqui):**
   - Vídeo A/B de 30 s (idle atual × spike), mesma luz/ângulo; Turing de
     mesa com ≥2 pessoas sem contexto: *"ele está olhando pra algo?"* e
     *"qual parece mais vivo?"* → **go** se o spike vence com clareza.
   - fps ≥ 30 mantido; heap/PSRAM idênticos ao baseline S2.6 (núcleos
     estáticos, sem malloc).
   - Reflexo de toque < 80 ms preservado com flag ligada.
   - **No-go:** arquivar S3.7 com a evidência, manter idle atual, sem
     débito.
6. **Execução:** declarar `S3.7` no início; commits pequenos, um ID;
   estrutura núcleo/casca obrigatória (`<comp>.c/.h` puro + `shell/` +
   `host_test/`); C17, `-Wall -Wextra -Werror`, prefixo `nb_`, guard
   `NB_<MODULO>_H`; constantes de tuning no núcleo, nunca na casca; nenhum
   pino ou hardware novo. Estimativa: ~0.5 dia núcleo+testes, ~0.5 dia
   casca+flag, ~0.5 dia bancada+vídeo.
7. **Após o go:** o restante da S3.7 (postura, motor de energia, campo
   contínuo, boca, variantes) será detalhado num "Plano S3.7 completo"
   separado — não ancorar a implementação em escopo que o spike pode matar.

**Execução (evidência registrada, 2026-07-06):**

- Núcleos `nb_breath`/`nb_attention` + host-tests, flag `NB_IDLE_V2_SPIKE`
  ligada por padrão, zero mudança fora do `idle_engine` (`main.c` intocado
  -- a troca de comportamento é interna a `idle_engine.c` via `#if`).
  Suíte completa de host-tests verde nas duas configs de flag (padrão e
  `NB_HOST_TEST_CFLAGS=-DNB_IDLE_V2_SPIKE=0`), incluindo o invariante
  X→IDLE do `tiny_fsm`. Build limpo (`idf.py build`) nas duas configs.
  Tremor de `nb_attention` retunado ao vivo em bancada (suavização +
  amplitude 0.02→0.06 + frequência de reajuste 150-350ms→600-1400ms —
  mesma classe de retune já documentada em `NB_IDLE_DRIFT_AMPLITUDE`).
- **fps/heap/PSRAM (log serial, N32R16V via COM5, flag ligada):** fps
  estável em 30.3 durante toda a janela observada; `heap_livre≈173 KB` e
  `psram_livre≈16,46 MB` sem variação entre amostras de heartbeat —
  gate de fps ≥ 30 fechado; heap/PSRAM sem sinal de vazamento na janela
  observada (comparação byte-a-byte contra o baseline do S2.6 não feita,
  só estabilidade dentro da própria janela).
- **Reflexo de toque (flag ligada):** 20 reações reais capturadas no log
  (`TAP`/`LONG_PRESS`/`SUSTAINED`), todas com `fsm_event=7` (TOUCH) e
  `latency_ms` entre 0 e 28 ms — folga grande sobre o critério de
  < 80 ms. Gate fechado.
- **Turing de mesa (vídeo A/B de 30 s, spike × idle atual, ≥2 pessoas sem
  contexto):** executado e confirmado pelo usuário — spike aprovado.
- **Decisão: GO.** Passo 0 do S3.7 fechado — todos os critérios de
  go/no-go do item 5 atendidos (Turing de mesa, fps/heap/PSRAM, reflexo de
  toque). Libera o "Plano S3.7 completo" abaixo.

**Plano S3.7 completo (pós-go, antes de implementar):**

Mesmo ID `S3.7` (governança do roadmap proíbe sub-IDs novos); sequência de
passos, cada um com núcleo puro + host-test + gate, do menor pro maior
risco — motores 100% dentro do `idle_engine` primeiro (mesmo padrão de
baixo risco do spike), renderer/`emotion_core` por último.

**Correções de escopo vs. o RFC** (fatos verificados no código, não no
doc — o RFC assume coisas que não são verdade hoje):

1. `nb_face_state_t` (`renderer.h:40-49`) **não tem boca** — é uma struct
   só de olhos (18 campos: curvatura, abertura, offset vertical, `x_off`,
   arredondamento, squint). Boca é renderer novo (C++/LovyanGFX), não
   fiação de campo já existente.
2. São **10 âncoras**, não 4 (`nb_face_expr_t`: `NEUTRAL, HAPPY, CURIOUS,
   SLEEPY, FOCUSED, SUSPICIOUS, SURPRISED, SAD, ALARMED, ANGRY`). Campo
   contínuo só nos 4 hubs (`NEUTRAL/HAPPY/SAD/ANGRY`); as outras 6
   continuam nearest-neighbor discreto, intocadas.
3. `nb_face_core_lerp()` (`renderer.c:50-72`) é só linear 2-pontos,
   escalar `t` — campo contínuo por posição `(v,a)` entre 4 hubs é um
   blend N-way novo (pode reaproveitar o helper por campo, não a função
   de pesos).
4. `circadian_core` não expõe hora do dia nem tempo-na-fase (só `phase`,
   `quiet_mode`, `brightness_scale`, `has_time_source`) — offset
   circadiano do vetor (RFC §7) precisa de campo novo.
5. `led_service` tem fase própria por estado da FSM (`state_elapsed_ms`),
   independente do clock do `idle_engine` — sincronizar respiração×LED é
   fiação nova em `main.c` (mesmo padrão de `circadian.brightness_scale`).
6. Não existe tracker de "tempo desde o último estímulo" em lugar nenhum
   — o motor de energia (tédio) parte do zero.

1. **Motor de postura** (`nb_posture.c/.h`, novo núcleo em `idle_engine`):
   a cada 30-90s, transição ~400ms pra nova micro-pose (roll ≤1°, gaze
   offset ≤0.05, assimetria ≤3%) que vira o novo repouso, nunca volta à
   pose exata (RFC §7, motor 2). Mesmo padrão de `nb_attention.c` (núcleo
   puro, RNG embutido, alvo re-sorteado + suavização exponencial).
   Invariante H7 crítico: entrada em IDLE reseta ao centro, deriva
   recomeça dali — host-test dedicado. Host-tests: nunca 2 poses
   consecutivas idênticas, envelope respeitado, reset limpo em IDLE. Gate:
   host-test verde, build limpo, zero mudança fora do `idle_engine`.
2. **Motor de energia** (`nb_energy.c/.h`, novo núcleo em `idle_engine`):
   circadiano + tédio + vetor modulam blink/pálpebra/gaze/sacada/
   respiração até `SLEEPING` (RFC §7, motor 3). Entrada de tédio: casca
   (`main.c`) alimenta "ms desde a última prioridade acima de
   BASELINE/IDLE_MOTIF" (já disponível via `nb_reflex_engine_shell_tick()`)
   por um novo parâmetro/setter, mesmo padrão de
   `nb_idle_engine_set_mode()`. Entrada circadiana: exige o campo novo do
   item 4 das correções — única exceção documentada a "zero mudança fora
   do idle_engine" nesta fase. `tiny_fsm` continua decidindo o corte pra
   `SLEEPING`; o motor só entrega o sinal contínuo. Gate: host-test verde,
   build limpo.
3. **Acoplamentos + blink unificado:** liga `nb_attention_set_saccade_callback()`
   a um blink de fato (blink×sacada); respiração em fase com o LED idle
   (fiação nova em `main.c`); roll segue gaze com ~100ms de atraso. Funde
   os processos de blink independente + gancho de sacada + motor de
   energia num agendador único. Host-tests: envelope de Poisson/refratário
   preservado (média 5s, refratário 1.8s, ~28% duplos); LED×respiração
   sincronizados dentro de tolerância. Gate: host-test verde, build
   limpo, confirmação visual em bancada (fase é coisa que só se vê ao
   vivo).
4. **Gestos nomeados** `CHECK_IN` (~1×/1-3min), `SLOW_BLINK`, `SIGH`:
   fisiologia/automanutenção (RFC §2, Regra da Causa — não é "atenção"
   fingida), slot exclusivo com motif/gesto em curso, `quiet_mode`
   dobra os tempos. Gate: host-test verde, build limpo.
   **Checkpoint intermediário:** com 1-4 fechados, os 3 motores do RFC §7
   estão completos e testados sem tocar renderer/`emotion_core` — bom
   ponto pra bancada intermediária (fps/heap/toque) antes do resto.
5. **Boca no renderer** (`nb_face_state_t` + `renderer.c` +
   `nb_face_renderer_shell`): campos de boca (abertura/curvatura — RFC
   §3), estender `nb_face_core_lerp()`, desenhar em LovyanGFX, novo
   parâmetro em `draw()`/`draw_dirty()` (hoje só `gaze_x/y, width_l/r,
   tilt, color`, sem gancho de boca). Sem host-test de "parece bom" — só
   bounds/interpolação numérica. Gate: build limpo + bancada (boca
   estática nos 4 hubs, sem campo contínuo ainda).
6. **Campo contínuo + temperamento + circadiano no vetor** (`emotion_core`):
   substitui `nb_emotion_core_nearest_expression()` por blend contínuo só
   entre os 4 hubs (fallback nearest-neighbor pras outras 6, sem
   regressão); temperamento (decay alvo em `(+0.10, +0.05)`); offset
   circadiano (campo novo do item 4). Host-tests do RFC §9: campo passa
   exatamente pelas âncoras calibradas e é contínuo entre elas; decay
   assimétrico fica pra S3.8. Gate: host-test verde, build limpo, bancada
   (as 6 âncoras fora do escopo não podem quebrar).
7. **Variantes episódicas** (2 por hub): sorteadas ao entrar na região,
   duram o episódio. Host-test RFC §9(5): nunca sai do envelope da região.
   Gate: host-test verde, build limpo, bancada (perceptível sem confundir
   a emoção-base).
8. **Aposentar o catálogo antigo (S2.4) + docs:** `NB_IDLE_V2_SPIKE` vira
   único caminho — remove `pick_long_motif`/`start_long_motif`/
   `sample_next_motif_ms` (hoje atrás de `#if !NB_IDLE_V2_SPIKE`) e os
   testes que dependiam deles. Atualiza `docs/VISUAL.md` §1-4 (RFC já
   declara esse escopo) e `docs/BEHAVIOR.md` §2/5. Registra que S2.2
   deixa de ser critério de paridade. Gate: host-tests verdes numa config
   só, build limpo, docs revisados.
9. **Gate final da S3.7:** host-tests 1-6 do RFC §9 verdes; bancada 60s
   (§9 — ≥2 blinks, ≥2 fixações >2s, respiração mensurável em vídeo,
   postura final ≠ inicial, zero ação intencional sem causa, nenhum
   intervalo de 15s idêntico); soak 48h modo pet; side-by-side v1
   registrando o novo baseline de persona.

Verificação por item: host-test do núcleo primeiro; suíte inteira
(`tools/run_host_tests.py`) verde, não só o componente tocado; build
limpo (`idf.py build`, prova compilação, não comportamento); itens com
saída visual (5, 6, 7, fase do LED no item 3) exigem confirmação em
bancada antes de fechar — nunca declarar `FEITO` só com build limpo.

### S4 — Voz (o robô conversa)

_Objetivo:_ pipeline de voz fim-a-fim com a mente.
_Dependências:_ S1.7, S3.6. _Referência:_ Voice Audio v2 do v1 (desenho) e
server v1 (refactor).

| ID   | Entrega                                                                                                                       | Gate de saída                                                                                                              | Status     |
| ---- | ----------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------- | ---------- |
| S4.1 | `audio_hal` I2S full-duplex 16 kHz (mic+spk no mesmo barramento)                                                              | loopback limpo; zero underrun em 30 min com render ativo (re-valida S0.3 na árvore real)                                   | `PENDENTE` |
| S4.1a | `display_hal`: otimizar flush SPI com staging SRAM suportado (dirty rects/filas medidas)                                    | medição registrada (fps/latência de flush/SRAM) antes×depois; soak sem regressão visual; fps ≥ 28 com áudio ativo; fatiamento documentado como solução permanente se mantido | `FEITO` |
| S4.2 | `wake_service` (WakeNet) + VAD (ESP-SR) com invariantes V-1..V-6 de `VOICE.md` §3 **como host-tests**                         | wake em ambiente real ≥ 9/10; falso-wake < 1/h; overlay listening < 250 ms; testes V-\* verdes                             | `PENDENTE` |
| S4.3 | Streaming NBP/2 de áudio (LISTEN*\* robô→server; SAY*\* server→robô; canal MEDIA com backpressure; barge-in físico por touch) | golden tests; sessão completa contra server fake; queda de link no meio da fala → fade ≤ 300 ms + IDLE                     | `PENDENTE` |
| S4.4 | Server: `TurnEngine` + `MindOutput` extraídos do orchestrator v1 (atores sobre bus, nenhum ator chama outro)                  | testes de turno portados do v1 passam na nova estrutura; barge-in cancela task de turno                                    | `PENDENTE` |
| S4.5 | Providers ligados: faster-whisper, Ollama/OpenAI com circuit breaker, Piper                                                   | conversa fim-a-fim em PT-BR; falha de LLM degrada com resposta honesta, sem travar FSM                                     | `PENDENTE` |
| S4.6 | Intents locais offline-first (hora, timer, status) respondendo sem LLM                                                        | intents respondem com LLM desligada; latência < 1 s                                                                        | `PENDENTE` |
| S4.7 | Gate de voz                                                                                                                   | budgets §4 de `QUALITY.md` medidos e registrados (wake→listening, fala→primeiro áudio); soak 24 h com conversas periódicas | `PENDENTE` |
| S4.8 | **Máscara de latência** (`docs/RFC-VIDA-V2.md` §6): `THINKING`/`EUREKA`/`CONFUSED` sobre o gap `LISTEN_STOP`→`SAY_START`, inferido localmente pela temporização das mensagens de S4.3 (zero mudança de protocolo). Dependências: S4.3–S4.5 + S3.7 | p95 do gap fala→primeiro áudio sem face estática (medido); timeout/queda de mente → `CONFUSED` → IDLE limpo; golden test da inferência local de turno | `PENDENTE` |

**Plano S4.1 (antes de implementar):**

1. `nb_hw_config.h` ganha as constantes do barramento I2S compartilhado
   (`HARDWARE.md`: BCLK 40, WS/LRCK 41, mic DIN 39/INMP441, speaker DOUT
   42/MAX98357A -- um único periférico I2S, full-duplex nativo via canais
   TX+RX no mesmo `i2s_port_t`, não dois barramentos).
2. `audio_hal` (`components/hal/audio_hal`, L0): núcleo C17 puro mínimo --
   só o que é matemática pura e reaproveitável (`nb_audio_hal_rms_s16()`
   pra medir nível de sinal, útil tanto pro ensaio de loopback quanto pro
   VAD leve de `wake_service` em S4.2). Sem lógica de ring
   buffer/backpressure aqui -- isso é `audio_service` (L3), fora de
   escopo de S4.1 (VOICE.md §5).
3. `shell/nb_audio_hal_shell.c/.h`: `i2s_new_channel()` com TX+RX no
   mesmo `I2S_NUM_0` (full-duplex automático por ambos os canais
   compartilharem config, API `driver/i2s_std.h` do ESP-IDF v5.5 --
   `i2s_channel_init_std_mode()` por canal com `clk_cfg` 16kHz/`slot_cfg`
   16-bit mono igual nos dois, `gpio_cfg` com `din`/`dout` trocados e
   `mclk`/pino não usado do lado oposto em `I2S_GPIO_UNUSED`). Atenção:
   a macro de slot mono do ESP32-S3 fixa `slot_mask=I2S_STD_SLOT_BOTH`
   independente do argumento -- `slot_mask` precisa ser sobrescrito
   explicitamente pra `I2S_STD_SLOT_LEFT` (ou o lado certo do INMP441)
   depois da macro default. `i2s_channel_register_event_callback()` com
   `on_recv_q_ovf`/`on_send_q_ovf` conta underrun/overflow (contador
   simples, exposto por getter) -- é a métrica direta do gate.
   `read()`/`write()` usam `i2s_channel_read`/`i2s_channel_write`
   (bloqueantes, com timeout).
4. `main.c`: task de bring-up temporária (mesmo padrão do touch em S3.1
   antes do reflex_engine existir) -- gera um tom de teste (ex. 440Hz)
   pro speaker enquanto lê o mic em loop, loga RMS do mic e contadores de
   underrun/overflow periodicamente (junto do heartbeat existente).
   Substituída quando `audio_service`/`wake_service` existirem (S4.2+).
5. `host_test`: `nb_audio_hal_rms_s16()` com senoide sintética
   (RMS conhecido analiticamente), silêncio (RMS=0), `NULL` seguro. Sem
   host-test de I2S real (é HAL de hardware, mesma regra de `led_hal`
   -- núcleo puro é só o RMS).
6. Gate: host-test verde + `idf.py build` limpo + ensaio de bancada real
   --toque físico/áudio: tom tocado é ouvido de verdade, RMS do mic capta
   sinal (não fica em zero/silêncio), zero underrun/overflow em 30 min
   com a task de face (render) ativa em paralelo -- fecha também o S0.3
   nunca executado (contenção render+I2S; SD fica de fora, componente
   `sdmmc` ainda não existe).

**Plano S4.1a (antes de implementar):**

Item de `display_hal`, não de áudio -- nasceu da contenção real de SRAM
interna DMA-capable entre o staging PSRAM→SRAM do SPI e os descritores DMA do
`audio_hal` (S4.1). A documentação oficial do ESP-IoT-Solution confirma que
o SPI driver não faz DMA direto a partir da PSRAM nesses targets: quando o
buffer vem da PSRAM, ele copia para SRAM antes de transferir. Portanto o
fatiamento em bandas (`nb_display_hal_shell.c`) é o caminho suportado e deve
ser tratado como solução permanente, não como gambiarra provisória nem como
algo a remover por "DMA direto de PSRAM".

1. Medir e registrar o baseline atual com áudio ativo: `S4.1` soak
   2026-07-06 em `docs/bringup/s4_1_audio_soak_20260706_040548.log` mostrou
   áudio limpo (`rx_ovf/tx_ovf/rx_timeout/tx_timeout=0`), mas render em
   ~20,8 fps por `flush_ms` ~40 ms.
2. A correção S4.1a remove o `esp_cache_msync()` do caminho normal: o
   renderer escreve no framebuffer PSRAM, o `display_hal` copia a região suja
   para SRAM interna DMA-capable, e o DMA lê esse staging interno.
3. Implementado em 2026-07-06: reduzir bytes por frame com dirty rectangles
   do renderer. O primeiro frame é full-screen; depois o renderer retorna a
   união conservadora entre olhos antigos e atuais. `face_demo` passou a
   registrar `flush_kb` junto de `fps/draw_ms/flush_ms`.
4. Implementado junto: staging fixo controlado pelo `display_hal`, em SRAM
   interna DMA-capable (~19 KB), preenchido por `memcpy` linha a linha a
   partir do framebuffer PSRAM. Isso evita depender de bounce transitório
   grande do driver e preserva uma única transferência em voo.
5. 60 MHz não é solução de produção nesta bancada: em 2026-07-06, 80 MHz
   apresentou artefatos; 60 MHz ficou visualmente normal, mas fps continuou
   ~20,8/20,9. O clock default voltou para 40 MHz estável.
6. Otimização terciária futura: experimentar enfileirar bandas mantendo o swap no
   próximo flush para recuperar sobreposição assíncrona, medindo heap interno
   mínimo para não recriar vários stagings SRAM concorrentes e voltar ao
   `ESP_ERR_NO_MEM`.
7. Não usar 80 MHz nesta bancada: apresentou artefatos, apesar de o limite
   teórico/documentado do SPI permitir essa frequência.
8. Gate: medição antes×depois de fps, `flush_ms`, `flush_kb`, heap interno mínimo e
   contadores de áudio; soak sem regressão visual; fps ≥ 28 com áudio ativo.

**Evidência S4.1a (2026-07-06):**

- Baseline com áudio ativo antes da correção, registrado em
  `docs/bringup/s4_1_audio_soak_20260706_040548.log`: render em ~20,8 fps,
  `flush_ms` ~40 ms, áudio limpo (`rx_ovf/tx_ovf/rx_timeout/tx_timeout=0`).
- Após dirty rectangles + staging DMA fixo (~19 KB), a bancada reportou:
  `face_demo: fps=30.3 logic_ms=0.26 draw_ms=7.44 flush_ms=15.06 flush_kb=54.9`
  com áudio simultâneo limpo:
  `audio_bringup: write_err=ESP_OK read_err=ESP_OK mic_rms=440.8 rx_ovf=0 tx_ovf=0 rx_timeout=0 tx_timeout=0`.
- Leitura técnica: a correção reduziu o volume médio transferido para ~54,9 KB
  por frame (contra 150 KB full-frame), derrubando o `flush_ms` para ~15 ms e
  recuperando ~30 fps sem regressão imediata no I2S.
- **Soak final executado (2026-07-06, N32R16V via COM5, 30 min):**
  `docs/bringup/s4_1a_display_audio_soak_20260706.log` registrou
  `elapsed_s=1810`, `lines=3139`, `face_samples=360`, `fps_avg=30.3003`,
  `fps_min=30.3`, `flush_ms_avg=15.1033`, `flush_kb_avg=55.0469`,
  `heap_min=170535`, `psram_min=16461580`, `audio_samples=360`,
  `rx_ovf_max=0`, `tx_ovf_max=0`, `rx_timeout_max=0`, `tx_timeout_max=0`,
  `panics=0`.
- O resumo automático contou `resets=1`, mas a única ocorrência de `rst:`
  no log é a linha inicial `rst:0x1 (POWERON)` logo após `=== soak started ===`,
  ou seja: boot capturado na abertura da serial, não reset no meio da janela.
  Não houve segunda sequência de boot nem `Guru Meditation`/panic durante os
  30 min.
- Gate de saída fechado: fps sustentado > 28 com áudio ativo, sem overflow/
  timeout, `heap_min`/`psram_min` registrados e sem regressão visual reportada
  na bancada. `S4.1a` encerrado: `FEITO`.

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
