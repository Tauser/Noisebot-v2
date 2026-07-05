idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES boot_manager watchdog wifi_setup ota mind_link display_hal renderer idle_engine emotion_core touch_service tiny_fsm event_bus reflex_engine led_service circadian_core schedule_core
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
