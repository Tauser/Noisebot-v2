# CLAUDE.md — NoiseBot 2

Instruções para o assistente de IA trabalhando neste repositório.
Este arquivo tem autoridade máxima sobre qualquer instrução geral.

---

## Projeto

**NoiseBot 2** é a segunda geração do companion robot NoiseBot: **um único
ESP32-S3** (Waveshare N32R16V — 32 MB flash / 16 MB PSRAM) como corpo +
sistema nervoso autonômico, e um server Python local como mente. Não existe
segundo MCU, não existe protocolo inter-MCU. O microSD é módulo externo
(SDMMC 1-bit). Câmera está **adiada** (form factor StackChan; slot SPI
CS9/MISO13 e mensagens `SNAPSHOT_*` reservados — não remover). A fonte de
verdade da execução é `docs/ROADMAP.md` (fases S0–S7).

Stack: ESP-IDF + FreeRTOS, C17 (C++ exclusivamente no renderer LovyanGFX),
CMake via `idf_component_register`. **Nunca usar Arduino.**
Server: Python ≥ 3.10, aiohttp, SQLite. Dashboard: React/Vite.

## Ambiente de Build (ESP-IDF)

Caminhos fixos da máquina de desenvolvimento — não buscar, usar diretamente:

```
IDF_PATH            = C:\esp\v5.5.4\esp-idf
IDF_PYTHON          = %USERPROFILE%\.espressif\python_env\idf5.5_py3.14_env\Scripts\python.exe
IDF_CMAKE           = C:\Espressif\tools\cmake\3.30.2\bin
IDF_NINJA           = C:\Espressif\tools\ninja\1.12.1
IDF_PYTHON_ENV_PATH = %USERPROFILE%\.espressif\python_env\idf5.5_py3.14_env
IDF_XTENSA          = C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin
```

**Atenção:** IDF v5.5.4 requer exatamente `esp-14.2.0_20260121`. Sempre incluir
`IDF_XTENSA` no PATH (versões próximas quebram fullclean + build).

Invocação padrão (PowerShell):

```powershell
$venv = "$env:USERPROFILE\.espressif\python_env\idf5.5_py3.14_env\Scripts\python.exe"
$env:IDF_PATH            = "C:\esp\v5.5.4\esp-idf"
$env:IDF_PYTHON_ENV_PATH = "$env:USERPROFILE\.espressif\python_env\idf5.5_py3.14_env"
$xtensaBin = "C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin"
$env:PATH  = "C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\ninja\1.12.1;$xtensaBin;$env:PATH"
Set-Location "D:\Projetos\Noisebot2\firmware"
& $venv "C:\esp\v5.5.4\esp-idf\tools\idf.py" <comando>
```

Placa do NB2: Waveshare ESP32-S3-DEV-KIT N32R16V — porta/MAC da unidade atual
serão registrados no gate S0.1 (uma unidade anterior morreu por curto
5V↔3V3; health check obrigatório). **Atenção: GPIO47/48 dessa placa são
domínio 1.8V.**

A Freenove CAM N16R8 (COM12, MAC `20:6e:f1:b2:3c:f4`) roda o **NoiseBot v1**
em produção: **nunca flashear** sem decisão explícita do usuário.

Testes do server: `cd server && pip install -e .[dev] && pytest`.

---

## Princípios (P1–P5) — ver `docs/PROJECT.md`

1. **Corpo e mente.** Cognição (conversa, memória, visão semântica) mora no
   server. Firmware carrega só o autonômico. Regra da mesa: se acontece com o
   robô sozinho na mesa → firmware; se precisa de linguagem/memória/raciocínio
   → server.
2. **Enforcement antes de feature.** Nada é `FEITO` sem gate executável.
3. **Núcleo funcional, casca imperativa.** Toda lógica não-trivial do firmware
   é lib C17 pura (sem FreeRTOS/ESP-IDF, clock e I/O injetados) + casca fina.
   Estrutura obrigatória: `<comp>.c/.h` puro, `shell/`, `host_test/`.
4. **Contrato é artefato.** Protocolo NBP/2 nasce de `protocol/nbp2.yaml` com
   codegen para C e Python. É proibido editar código gerado ou duplicar
   constantes de protocolo à mão.
5. **Complexidade só depois de medir.** Gasto estrutural exige medição
   registrada em doc antes.

## Regras Inegociáveis

### Código

- C17 em tudo, exceto o renderer (`components/services/face/renderer/` em C++
  atrás de wrapper `extern "C"`). Nenhum C inclui header C++.
- `-Wall -Wextra -Werror` — zero warnings.
- Prefixo `nb_` para tipos, eventos e macros globais; funções de componente
  seguem `<componente>_<operação>`.
- Include guard `#ifndef NB_<MODULO>_H`. Erros via `esp_err_t`;
  `ESP_ERROR_CHECK` só em init.
- Constantes de hardware apenas em `firmware/components/hal/nb_hw_config.h` —
  nunca hardcoded em lógica. Pinout congela na tag `pinout-v1.0` (S0.4);
  depois disso, mudança de pino exige RFC no ROADMAP. GPIO47/48 (1.8V) nunca
  recebem lógica 3.3V.

### Arquitetura (ver `docs/ARCHITECTURE.md`)

- 5 camadas: HAL → Infra → Safety → Serviços → Autonômico. Camada chama só
  para baixo; cross-layer só via event bus; HAL não publica no bus.
- Nenhum `malloc` em ISR, task de safety ou render loop. Pool estático de
  eventos com slots reservados de safety.
- Nenhum framebuffer em SRAM (sprites e câmera em PSRAM). I2S DMA em SRAM.
- Escrita em SD nunca síncrona em task com prioridade ≥ 10 (worker assíncrono).
- `IDLE` é o baseline persistente: `NEUTRAL`, gaze central, pescoço central,
  LED idle, zero overlays. Toda entrada em IDLE limpa estado transitório — e o
  host-test de invariante X→IDLE deve cobrir 100% das transições, sempre.

### Motion Safety — veto absoluto

- Nenhum movimento de servo antes de `motion_safety` verde **e** do gate
  elétrico da fase S6.1 assinado (o v1 perdeu uma placa por curto 5V↔3V3).
- Toda escrita de posição passa por `motion_safety_check_position()`.
- Stall, subtensão, heartbeat timeout e brownout-disable são inegociáveis.
  FAULT é pegajoso: exige reset. Motion tem dois perfis registrados
  (`HARDWARE.md` §Perfis de motion): no perfil B (MG90S, sem feedback),
  stall é proxy de corrente via INA219 + corte do trilho (GPIO3) e
  temperatura é coberta por limite de duty — lacunas documentadas, nunca
  relaxamento silencioso. `servo_hal` é interface dupla por capability.

### Segurança (ver `docs/SECURITY.md`)

- Nenhum segredo em código, log de produção ou git (CI roda scan de segredos).
- Token NBP/2 obrigatório no HELLO, comparação timing-safe nos dois lados.
- Credenciais WiFi só via provisioning SoftAP → NVS. Proibido `wifi_creds.h`.
- OTA apenas assinada, com anti-rollback, após S1.8.
- Sem TLS no firmware (mbedTLS não cabe): rede é LAN local, HTTP local apenas.

### Qualidade (ver `docs/QUALITY.md`)

- CI é pré-condição de merge. Workflows **sem filtro de paths** para testes
  que cruzam firmware↔server (lição v1: gate morto em silêncio).
- Definition of done: código + host-test do núcleo + entrada no schema (se
  toca protocolo) + budget respeitado + 5 linhas de doc no componente.
- Testes de contrato comparam **artefatos** (bytes gerados, structs), nunca
  regex sobre código-fonte.

## Governança do Roadmap

- Só existem os IDs `S*.*` registrados em `docs/ROADMAP.md`. É proibido criar
  fases/subfases não registradas; se o trabalho não cabe em um ID, parar e
  propor alteração do documento com motivo, dependências e gate.
- Uma subfase só vira `FEITO` com todos os critérios de saída atendidos e
  evidência registrada. Build limpo prova compilação — não prova hardware,
  latência nem comportamento.
- Spike não autoriza fase seguinte. Commits pequenos, um ID por commit.
- O firmware v1 (repo `noisebot`, D:\Projetos\Noisebot) continua sendo o
  produto em operação até S7; consultar o fonte dele como referência é
  encorajado, copiar sem reescrever pelo padrão núcleo/casca não é.

## Workflow

- Não usar skills GSD neste projeto. Trabalho inline, planejamento explícito.
- Ao iniciar trabalho, declarar o ID do roadmap. Ao terminar, listar gates
  executados e pendentes.
- Idioma da documentação: PT-BR. Código e identificadores: inglês.
