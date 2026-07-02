cmake_minimum_required(VERSION 3.16)

# NoiseBot 2 — firmware single-MCU (Waveshare ESP32-S3-DEV-KIT N32R16V)
# Regra: -Wall -Wextra -Werror em todo código do projeto (ver CLAUDE.md).

set(EXTRA_COMPONENT_DIRS
    "components/infra/event_bus"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Warnings como erro apenas para componentes do projeto (não para o IDF).
idf_build_set_property(COMPILE_OPTIONS "-Wall;-Wextra" APPEND)

project(noisebot2)
