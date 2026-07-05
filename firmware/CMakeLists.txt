cmake_minimum_required(VERSION 3.16)

# NoiseBot 2 — firmware single-MCU (Waveshare ESP32-S3-DEV-KIT N32R16V)
# Regra: -Wall -Wextra -Werror em todo código do projeto (ver CLAUDE.md).

set(EXTRA_COMPONENT_DIRS
    "components/autonomic/emotion_core"
    "components/autonomic/idle_engine"
    "components/autonomic/tiny_fsm"
    "components/autonomic/reflex_engine"
    "components/hal/display_hal"
    "components/hal/touch_hal"
    "components/services/touch_service"
    "components/infra/event_bus"
    "components/infra/logger"
    "components/infra/app_config"
    "components/infra/boot_manager"
    "components/infra/watchdog"
    "components/infra/wifi_setup"
    "components/infra/ota"
    "components/infra/nbp2"
    "components/services/mind_link"
    "components/services/face/renderer"
    "components/services/face/renderer/vendor/LovyanGFX"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Warnings como erro apenas para componentes do projeto (não para o IDF).
idf_build_set_property(COMPILE_OPTIONS "-Wall;-Wextra" APPEND)

project(noisebot2)
