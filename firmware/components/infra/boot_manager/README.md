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

**Pendente (fora do escopo desta fatia, ver `docs/ROADMAP.md` §S1.4):** casca
que orquestra a inicialização real (`logger`, `app_config`, `event_bus`, ...)
usando este núcleo e mede boot→idle real (gate: < 3 s).
