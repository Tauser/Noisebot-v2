# logger

Núcleo C17 puro do logger estruturado (S1.3). Sem FreeRTOS, sem `esp_*`, sem
`malloc`; timestamp é injetado pelo chamador.

Contrato do núcleo:

- ring circular de 128 entradas (nível, módulo, timestamp, mensagem de
  tamanho fixo, sequência monotônica); cheio, sobrescreve a mais antiga e
  conta o drop — logar nunca bloqueia nem falha por causa do ring cheio;
- filtro de nível (`min_level`) descarta antes de consumir sequência;
- `nb_logger_copy_all()` tira um snapshot completo sem alterar estado (dump
  de shutdown/panic); `nb_logger_drain_since()` lê incrementalmente por
  cursor de sequência (worker de SD) e sinaliza gap se o cursor ficou para
  trás do que já foi sobrescrito.

**Pendente (fora do escopo desta fatia, ver `docs/ROADMAP.md` §S1.3):** casca
com mutex/task, worker SD (depende de S0.3 — sdmmc HAL ainda não existe) e
hook de dump em panic (precisa validação em bancada do mecanismo do ESP-IDF
v5.5.4 antes de virar código). O núcleo não assume concorrência interna: a
casca futura deve serializar `nb_logger_log` com mutex ou task dona do
logger.
