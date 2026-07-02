# Componentes do firmware

Organização por camada (`docs/ARCHITECTURE.md` §2). Cada componente com lógica
não-trivial segue o padrão obrigatório núcleo/casca (P3):

```
<componente>/
├── <componente>.c / .h    # núcleo C17 puro (sem FreeRTOS/esp_*), clock injetado
├── shell/                 # casca: task, filas, drivers, event bus
└── host_test/             # testes do núcleo, rodam no host em CI
```

| Diretório | Camada | Fase de entrada |
| --- | --- | --- |
| `hal/` | L0 — drivers (display, audio_i2s, servo_1wire, led_rmt, touch, camera, sdmmc, wifi) + `nb_hw_config.h` | S1–S6 |
| `infra/` | L1 — event_bus, config, logger, watchdog, boot_manager, ota | S1 |
| `safety/` | L2 — motion_safety (veto), power_monitor | S6 (núcleo pode nascer antes, host-only) |
| `services/` | L3 — face, audio, motion, led, touch, camera, storage, mind_link | S2–S6 |
| `autonomic/` | L4 — tiny_fsm, emotion_core, idle_engine, reflex_engine, schedule_core | S2–S3 |

Regras que o review verifica em todo PR:

- camada só chama para baixo; cross-layer só via event bus;
- HAL não publica no bus;
- nenhum `malloc` em ISR/safety/render;
- constantes de hardware apenas em `hal/nb_hw_config.h`;
- núcleo novo sem `host_test/` não passa (Definition of Done, `docs/QUALITY.md`).
