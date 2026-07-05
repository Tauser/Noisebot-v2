idf_component_register(
    SRCS "mind_link.c" "shell/nb_mind_link_shell.c" "shell/nb_mind_link_token_shell.c"
    INCLUDE_DIRS "." "shell"
    REQUIRES nbp2 esp_timer nvs_flash esp_netif event_bus
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
