idf_component_register(
    SRCS "renderer.c" "shell/nb_face_renderer_shell.cpp"
    INCLUDE_DIRS "." "shell"
    REQUIRES display_hal LovyanGFX
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
