# ota

Nucleo C17 puro e casca ESP-IDF para OTA A/B assinada (S1.8).

O nucleo modela apenas a politica local executavel: uma imagem candidata
nunca pode ter `secure_version` menor que a imagem em execucao, uma imagem
pendente so e confirmada depois que o boot do esqueleto fica saudavel, e
estado pendente sem boot saudavel nao e commitado. Ele nao chama ESP-IDF,
FreeRTOS, NVS nem aloca memoria.

Casca (`shell/nb_ota_shell.c/.h`):

- le o estado da particao atual via `esp_ota_get_state_partition`;
- se a imagem estiver em `ESP_OTA_IMG_PENDING_VERIFY`, chama
  `esp_ota_mark_app_valid_cancel_rollback()` somente depois do boot inicial
  da aplicacao;
- dispoe `nb_ota_shell_rollback_and_reboot()` para o caminho futuro de falha
  explicita de health check.

O download da imagem por NBP/2 (`OTA_BEGIN`/`OTA_CHUNK`/`OTA_END`) entra na
proxima fatia da S1.8, depois que S1.7 fechar o payload/servidor fake. Esta
fatia prepara a confirmacao/rollback local e o gate de assinatura.

Pendencias de bancada para fechar S1.8:

- habilitar perfil seguro de build com chave offline;
- executar OTA ida-e-volta entre `ota_0` e `ota_1`;
- provar que imagem adulterada e recusada antes do boot;
- provar dump de flash com token NBP/2 ilegivel apos flash encryption;
- assinar explicitamente a queima de eFuses da unidade N32R16V.
