idf_component_register(
    SRCS "app_config.c" "shell/nb_app_config_shell.c"
    INCLUDE_DIRS "." "shell"
    REQUIRES freertos nvs_flash
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
