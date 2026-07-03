idf_component_register(
    SRCS "ota.c" "shell/nb_ota_shell.c"
    INCLUDE_DIRS "." "shell"
    REQUIRES app_update
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
