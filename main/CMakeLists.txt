idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp32-camera esp_http_server nvs_flash esp_wifi esp_event esp_timer
                                 driver freertos log esp_netif)