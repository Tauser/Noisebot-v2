idf_component_register(
    SRCS "event_bus.c" "shell/nb_event_bus_shell.c"
    INCLUDE_DIRS "." "shell"
    PRIV_REQUIRES freertos
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
