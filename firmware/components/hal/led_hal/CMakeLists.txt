idf_component_register(
    SRCS "shell/nb_led_hal_shell.c"
    INCLUDE_DIRS "shell"
    REQUIRES nb_hw_config led_strip
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
