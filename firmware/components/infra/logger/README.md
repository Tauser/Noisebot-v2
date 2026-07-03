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

Casca (`shell/nb_logger_shell.c/.h`, FreeRTOS): singleton `nb_logger_t`
protegido por mutex (`xSemaphoreCreateMutex`), com timestamp real via
`esp_timer_get_time()`. Serializa `log/set_level/get_stats/copy_all/
drain_since` — seguro para chamada concorrente de múltiplas tasks. Chamadas
antes de `nb_logger_shell_init()` retornam falso/vazio sem travar.

**Pendente (fora do escopo desta fatia, ver `docs/ROADMAP.md` §S1.3):** worker
SD (depende de S0.3 — sdmmc HAL ainda não existe, aguardando módulo microSD
físico) e hook de dump em panic (precisa validação em bancada do mecanismo do
ESP-IDF v5.5.4 antes de virar código). Não há task dedicada ainda — sem
worker para alimentar, uma task própria seria complexidade sem necessidade
medida (P5); ela entra junto do worker SD.
