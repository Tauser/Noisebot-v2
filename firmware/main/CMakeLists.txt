idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
)

# -Werror no código do projeto (componentes próprios seguem o mesmo padrão).
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
