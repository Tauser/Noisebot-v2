idf_component_register(
    SRCS "display_hal.c" "shell/nb_display_hal_shell.c"
    INCLUDE_DIRS "." "shell"
    REQUIRES esp_lcd driver esp_mm
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
