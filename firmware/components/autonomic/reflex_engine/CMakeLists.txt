idf_component_register(
    SRCS "reflex_engine.c" "shell/nb_reflex_engine_shell.c"
    INCLUDE_DIRS "." "shell"
    REQUIRES tiny_fsm emotion_core event_bus touch_service
    PRIV_REQUIRES esp_timer
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
