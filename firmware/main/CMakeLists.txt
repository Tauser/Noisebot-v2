idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES boot_manager watchdog wifi_setup mind_link
)

# -Werror no código do projeto (componentes próprios seguem o mesmo padrão).
component_compile_options(
    -Wall
    -Wextra
    -Werror
    -Werror=unused-function
    -Werror=unused-variable
    -Werror=unused-but-set-variable
    -Werror=deprecated-declarations
)
