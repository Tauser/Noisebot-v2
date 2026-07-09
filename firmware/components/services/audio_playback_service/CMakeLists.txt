idf_component_register(
    SRCS "audio_playback_service.c" "shell/nb_audio_playback_service_shell.c"
    INCLUDE_DIRS "." "shell"
    REQUIRES log
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
