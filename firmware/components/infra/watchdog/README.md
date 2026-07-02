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

Nesta fatia só existe `app_main` como task de produto ativa. Cada task nova
de S2+ deve entrar no watchdog shell no mesmo commit em que nascer, com feed
no loop natural da própria task.
