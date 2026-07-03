cmake_minimum_required(VERSION 3.16)

# NoiseBot 2 — firmware single-MCU (Waveshare ESP32-S3-DEV-KIT N32R16V)
# Regra: -Wall -Wextra -Werror em todo código do projeto (ver CLAUDE.md).

set(EXTRA_COMPONENT_DIRS
    "components/hal/display_hal"
    "components/infra/event_bus"
    "components/infra/logger"
    "components/infra/app_config"
    "components/infra/boot_manager"
    "components/infra/watchdog"
    "components/infra/wifi_setup"
    "components/infra/ota"
    "components/infra/nbp2"
    "components/services/mind_link"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Warnings como erro apenas para componentes do projeto (não para o IDF).
idf_build_set_property(COMPILE_OPTIONS "-Wall;-Wextra" APPEND)

project(noisebot2)
