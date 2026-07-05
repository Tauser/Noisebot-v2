idf_component_register(
    SRCS "circadian_core.c" "shell/nb_circadian_core_shell.c"
    INCLUDE_DIRS "." "shell"
    REQUIRES tiny_fsm app_config
    PRIV_REQUIRES esp_timer
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
