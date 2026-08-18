idf_component_register(
    SRCS
        "main.c"
        "game.c"
        "leds.c"
        "buttons.c"
        "web_server.c"
    INCLUDE_DIRS "../include"
    REQUIRES
        nvs_flash
        esp_wifi
        esp_netif
        esp_http_server
        esp_event
        led_strip
        driver
)
