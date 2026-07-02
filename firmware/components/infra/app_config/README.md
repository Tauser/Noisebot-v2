# app_config

Núcleo C17 puro do config tipado (S1.4). Sem FreeRTOS, sem `esp_*`, sem
`malloc`, sem NVS — cache em RAM validado contra uma tabela central de
chaves.

Componente/pasta chamados `app_config` (não `config`) porque o próprio
ESP-IDF já tem um componente interno chamado `config` (geração de
`sdkconfig.h`); usar o mesmo nome causaria colisão de alvo no CMake assim que
algo passasse a depender deste componente via `REQUIRES`.

Contrato do núcleo:

- tabela central de chaves (`nb_config_key_t` → `nb_config_descriptor_t`:
  nome, tipo, default, min/max) em `app_config.c` — nenhuma chave solta em
  outro lugar do código;
- getters/setters tipados por chave (`nb_config_get_u32`/`nb_config_set_u32`,
  `..._i32`); chamar o accessor do tipo errado para uma chave é rejeitado,
  assim como valor fora da faixa do descritor — nos dois casos sem alterar o
  valor anterior;
- `get` devolve o default da chave enquanto ela nunca foi setada.

Chaves atuais: `NB_CONFIG_KEY_LOG_MIN_LEVEL` (nível mínimo do `logger`,
0..4) e `NB_CONFIG_KEY_BOOT_FAIL_STREAK` (contador de boots consecutivos com
falha crítica, para o `boot_manager`). Chave nova só entra quando a feature
que a usa existir.

Casca (`shell/nb_app_config_shell.c/.h`, FreeRTOS + NVS): singleton
protegido por mutex. `nb_app_config_shell_init()` chama `nvs_flash_init()`
(com erase+retry se a partição estiver corrompida/versão nova) e abre o
namespace `nb_cfg`; carrega cada chave conhecida do NVS para o cache (chave
ausente = mantém default). `set_*` só grava em NVS (`nvs_set_*` +
`nvs_commit`) quando o valor validado difere do anterior.

**Pendente (fora do escopo desta fatia, ver `docs/ROADMAP.md` §S1.4):** nada
pendente neste componente — a persistência está completa. O que falta é o
`boot_manager` orquestrando tudo em bancada e o gate de saída da subfase.
