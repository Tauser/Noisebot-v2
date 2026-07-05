# led_hal

Componente `firmware/components/hal/led_hal` — S3.3, camada L0
(`ARCHITECTURE.md`). Casca fina sobre `espressif/led_strip` (RMT),
GPIO21 (`nb_hw_config.h`), `NB_HW_LED_COUNT` WS2812 em cadeia. Primeiro
uso do component manager no v2 (`idf_component.yml`, mesma dependência do
`nb_hal` da v1).

Sem lógica não-trivial pra extrair — só inicializa o driver e escreve
pixels já gamma-corrigidos pelo `led_service` (L3, núcleo puro). Por isso
não segue o padrão núcleo/casca de P3: é só `shell/`, sem `.c/.h` de
núcleo nem `host_test` (nada pra testar sem hardware real).
