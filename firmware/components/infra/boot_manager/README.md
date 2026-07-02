# boot_manager

Núcleo C17 puro do boot_manager (S1.4). Sem FreeRTOS, sem `esp_*`, sem
`malloc`; timestamps de início/fim de cada fase são injetados pelo chamador.

Contrato do núcleo:

- sequência de fases nomeadas (`begin_phase`/`end_phase`) com criticidade
  (`CRITICAL`/`NON_CRITICAL`); só uma fase aberta por vez, capacidade fixa de
  `NB_BOOT_MANAGER_MAX_PHASES` (16);
- relatório determinístico: fase, criticidade, sucesso, duração e duração
  total somada;
- outcome `BOOT_OK` enquanto nenhuma fase crítica falhar; qualquer fase
  crítica com `success=false` vira `SAFE_MODE` e registra o nome da fase
  como motivo — motivo pegajoso, não é sobrescrito por falhas seguintes
  (mesmo espírito do `FAULT` pegajoso do motion safety);
- falha em fase não-crítica não afeta o outcome.

Casca (`shell/nb_boot_manager_shell.c/.h`): `nb_boot_manager_shell_run()`
orquestra, em sequência, a inicialização real de `logger` (crítico) e
`app_config` (crítico), medindo cada fase com `esp_timer`. Em `SAFE_MODE`,
incrementa `NB_CONFIG_KEY_BOOT_FAIL_STREAK` (melhor esforço); em `BOOT_OK`,
zera o contador. Chamado por `main.c` no início do `app_main`.

`event_bus` **não** entra nessa sequência ainda — seu próprio README diz que
a casca dele só nasce quando houver serviço publicando evento, e ela não
existe; incluí-lo aqui seria inverter a ordem de dependência (criar a casca
do event_bus a reboque do boot_manager, não porque um serviço precisa dela).

**Pendente (fora do escopo desta fatia, ver `docs/ROADMAP.md` §S1.4):** medir
boot→idle real em bancada (gate: < 3 s) e forçar falha de fase crítica em
hardware para confirmar `SAFE_MODE` — os dois são o gate de saída da
subfase, não coisa que dá para provar só com host-test/build.
