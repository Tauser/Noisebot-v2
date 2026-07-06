idf_component_register(
    SRCS "idle_engine.c" "nb_breath.c" "nb_attention.c" "nb_posture.c"
    INCLUDE_DIRS "."
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
