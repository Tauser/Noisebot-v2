# nb_hw_config

Componente header-only `firmware/components/hal/nb_hw_config` — centraliza
constantes de hardware (GPIO, canais) fora de qualquer lógica, conforme
CLAUDE.md §Código. Primeira instância criada em S3.3 (`led_hal`), migrando
junto o GPIO2 do `touch_hal` que antes vivia hardcoded no `shell/`.

Sem `.c`, sem dependência de ESP-IDF/FreeRTOS — só o header. Pinout
congelado na tag `pinout-v1.0` (S0.4); mudança de pino depois disso exige
RFC no ROADMAP.
