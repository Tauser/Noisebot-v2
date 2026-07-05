idf_component_register(
    SRCS "touch_hal.c" "shell/nb_touch_hal_shell.c"
    INCLUDE_DIRS "." "shell"
    REQUIRES esp_driver_touch_sens nb_hw_config
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
