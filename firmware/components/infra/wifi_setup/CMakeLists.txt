idf_component_register(
    SRCS "wifi_setup.c" "shell/nb_wifi_setup_shell.c"
    INCLUDE_DIRS "." "shell"
    REQUIRES esp_wifi esp_netif esp_event nvs_flash wifi_provisioning
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
