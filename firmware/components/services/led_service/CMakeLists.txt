idf_component_register(
    SRCS "led_service.c" "shell/nb_led_service_shell.c"
    INCLUDE_DIRS "." "shell"
    REQUIRES tiny_fsm led_hal
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
