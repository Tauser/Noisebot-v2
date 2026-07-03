# watchdog

Componente L1 de infraestrutura para S1.5.

O núcleo C17 puro (`watchdog.c/.h`) mantém o cadastro determinístico de tasks,
último feed, timeout por task e causa de reset observada. Ele não chama
FreeRTOS, ESP-IDF, NVS nem aloca memória; o host-test cobre feed, timeout,
duplicidade, capacidade e causa persistível.

A casca (`shell/nb_watchdog_shell.c/.h`) inicializa o TWDT do ESP-IDF com
panic/reset habilitado, assina a task chamadora (`app_main`) e grava no NVS
namespace `nb_wdog` o `esp_reset_reason()` e a causa classificada do boot
anterior. O TWDT monitora também as idle tasks via `idle_core_mask`, então
travamentos que impedem o escalonamento viram reset de hardware.

**Achado de bancada:** `sdkconfig` tem `CONFIG_ESP_TASK_WDT_INIT=y`, então o
ESP-IDF já inicializa a TWDT do sistema antes do nosso `app_main` rodar —
`esp_task_wdt_init()` sempre retorna `ESP_ERR_INVALID_STATE` neste projeto.
O default do sdkconfig tem `CONFIG_ESP_TASK_WDT_PANIC` **desligado**; se a
casca só ignorasse esse erro (como numa versão inicial deste componente),
nossa escolha de `trigger_panic=true`/timeout nunca seria aplicada e uma task
travada de verdade não reiniciaria o robô. A correção: em
`ESP_ERR_INVALID_STATE`, chamar `esp_task_wdt_reconfigure()` para aplicar a
config desejada na TWDT já ativa.

Nesta fatia só existe `app_main` como task de produto ativa. Cada task nova
de S2+ deve entrar no watchdog shell no mesmo commit em que nascer, com feed
no loop natural da própria task.
