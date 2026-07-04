# touch_hal

Componente `firmware/components/hal/touch_hal` — S3.1, camada L0
(`ARCHITECTURE.md`). Núcleo C17 puro (`touch_hal.c/.h`): só a média usada
pra calcular o baseline de calibração de boot (`nb_touch_hal_compute_
baseline()`), `NULL`/`count==0` seguros.

Casca (`shell/nb_touch_hal_shell.c/.h`): periférico touch nativo do
ESP-IDF (`driver/touch_sens.h`, API v2 do ESP32-S3), GPIO2 / canal 2
(`HARDWARE.md`: "TOUCH2", fita de cobre validada no v1). `active_thresh`
do canal fica alto e inerte de propósito — a detecção real de toque roda
no `touch_service` (núcleo puro), não no mecanismo de ativação de
hardware; a casca só lê o dado suavizado (`TOUCH_CHAN_DATA_TYPE_SMOOTH`).

Calibração de boot: 200ms de settle (não tocar o sensor) + 10 amostras
em 100ms, mesma sequência validada no v1.
