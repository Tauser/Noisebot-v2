idf_component_register(
    SRCS "touch_service.c" "shell/nb_touch_service_shell.c"
    INCLUDE_DIRS "." "shell"
    REQUIRES touch_hal
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
