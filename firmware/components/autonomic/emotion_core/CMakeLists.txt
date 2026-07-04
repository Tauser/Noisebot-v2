idf_component_register(
    SRCS "emotion_core.c"
    INCLUDE_DIRS "."
    REQUIRES renderer
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
