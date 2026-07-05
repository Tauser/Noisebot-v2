idf_component_register(
    SRCS "audio_hal.c" "shell/nb_audio_hal_shell.c"
    INCLUDE_DIRS "." "shell"
    REQUIRES nb_hw_config esp_driver_i2s
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
