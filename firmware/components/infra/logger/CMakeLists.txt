idf_component_register(
    SRCS "logger.c" "shell/nb_logger_shell.c"
    INCLUDE_DIRS "." "shell"
    REQUIRES freertos esp_timer
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
