idf_component_register(
    SRCS "boot_manager.c" "shell/nb_boot_manager_shell.c"
    INCLUDE_DIRS "." "shell"
    REQUIRES esp_timer logger app_config
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
