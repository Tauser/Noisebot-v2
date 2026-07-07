idf_component_register(
    SRCS "wake_service.c" "shell/nb_wake_service_shell.c"
    INCLUDE_DIRS "." "shell"
    REQUIRES event_bus esp_timer log
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
