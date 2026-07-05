idf_component_register(
    SRCS "schedule_core.c" "shell/nb_schedule_core_shell.c"
    INCLUDE_DIRS "." "shell"
    REQUIRES circadian_core
    PRIV_REQUIRES nvs_flash
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
