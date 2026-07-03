# Componente "nbp2": casca ESP-IDF fininha em volta do C gerado a partir de
# protocol/nbp2.yaml. Não gera nada aqui (o codegen precisa de PyYAML, que
# não é dependência do toolchain do ESP-IDF) — só compila
# protocol/generated/c/nbp2.c/.h, que precisam existir no disco ANTES do
# build (rode `python protocol/codegen/generate_nbp2.py` primeiro; o CI faz
# isso no job firmware-build antes da action do ESP-IDF, com Python comum do
# runner, não com o Python do IDF). Ver README.md deste componente.

set(NBP2_GENERATED_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../../protocol/generated/c")

if(NOT EXISTS "${NBP2_GENERATED_DIR}/nbp2.c")
    message(FATAL_ERROR
        "nbp2: protocol/generated/c/nbp2.c nao existe. Rode primeiro: "
        "python protocol/codegen/generate_nbp2.py (precisa de `pip install pyyaml`).")
endif()

idf_component_register(
    SRCS "${NBP2_GENERATED_DIR}/nbp2.c"
    INCLUDE_DIRS "${NBP2_GENERATED_DIR}"
)

component_compile_options(
    -Wall
    -Wextra
    -Werror
    -Werror=unused-function
    -Werror=unused-variable
    -Werror=unused-but-set-variable
    -Werror=deprecated-declarations
)
