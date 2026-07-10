# S4.2 — flash/smoke do wake_service (2026-07-06)

## Contexto

Validação intermediária da fase `S4.2` após a entrada do novo componente
`wake_service` e da integração mínima com `event_bus`/`reflex_engine`.
Objetivo desta rodada: provar que a imagem compila, flashea e sobe na
Waveshare N32R16V sem regressão óbvia de boot.

## Unidade de bancada

- Placa: Waveshare ESP32-S3-DEV-KIT N32R16V
- Porta usada: `COM5`
- MAC lido pelo `esptool`: `90:e5:b1:cc:3d:58`

## Comando de flash

Ambiente conforme `CLAUDE.md`/`AGENTS.md`:

```powershell
$env:IDF_PATH            = "C:\esp\v5.5.4\esp-idf"
$env:IDF_PYTHON_ENV_PATH = "C:\Espressif\tools\python\v5.5.4\venv"
$env:IDF_TOOLS_PATH      = "C:\Espressif\tools"
$env:PATH                = "C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\xtensa-esp-elf\esp-14.2.0_20260121\xtensa-esp-elf\bin;$env:PATH"
& "C:\Espressif\tools\python\v5.5.4\venv\Scripts\python.exe" `
  "C:\esp\v5.5.4\esp-idf\tools\idf.py" -p COM5 flash
```

## Resultado

- `idf.py build` verde antes do flash.
- `idf.py -p COM5 flash` concluído com `Done`.
- `noisebot2.bin` gravado em `0x20000`.
- Tamanho reportado da app: `0x106990` bytes; menor partição app com ~74%
  livre.
- A placa permaneceu **funcional** após o flash (confirmação manual do
  usuário em bancada).

## Observações

- O monitor serial local não pôde ser usado como evidência final nesta
  rodada porque a `COM5` ficou sendo reaberta por outro processo/ferramenta
  fora do fluxo do `idf.py`, gerando `PermissionError(13, 'Acesso negado.')`
  em tentativas posteriores de `idf.py monitor`.
- Para repetir a próxima rodada de bancada com o harness de S4.2 sem tocar
  no `sdkconfig` principal, usar o perfil dedicado:
  `sdkconfig.defaults;sdkconfig.s4_2_wake_bench.defaults`.
- Desde 2026-07-09, a imagem também passa a logar telemetria explícita do
  harness (`wake_state`, contagem de wakes, `LISTEN_START`, latência
  `WAKE`→`LISTEN_START`, misses do budget de 250 ms, feedbacks honestos e
  motivos de `LISTEN_END`) pela própria task `audio_bringup`, facilitando a
  próxima rodada de medição formal.
- Como a validação prática de funcionalidade veio do usuário na bancada,
  esta rodada prova **boot funcional pós-flash**, mas **não** fecha ainda os
  budgets específicos do gate S4.2 (`wake >= 9/10`, falso wake `<1/h`,
  overlay `<250 ms`), nem substitui a coleta de logs seriais quando a porta
  estiver livre.

## Leitura para o roadmap

Esta evidência fecha a parte "imagem sobe na placa" da integração atual do
`wake_service` e reduz o risco da próxima fatia: produtor real de wake/VAD
ou harness de bancada, ainda pendente.
