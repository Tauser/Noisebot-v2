idf_component_register(
    SRCS "watchdog.c" "shell/nb_watchdog_shell.c"
    INCLUDE_DIRS "." "shell"
    REQUIRES freertos esp_timer esp_system nvs_flash
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
