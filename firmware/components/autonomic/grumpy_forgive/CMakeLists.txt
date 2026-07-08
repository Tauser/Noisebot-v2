idf_component_register(
    SRCS "grumpy_forgive.c"
    INCLUDE_DIRS "."
    REQUIRES arc_core
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
