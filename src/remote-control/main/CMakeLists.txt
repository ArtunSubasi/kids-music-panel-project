idf_component_register(SRCS "media_mapping.c" "gpio_example_main.c" "display/display.c" "rfid/rfid_scanner.c"
                    INCLUDE_DIRS "." "common" "display" "rfid"
                    REQUIRES esp_wifi esp_netif nvs_flash esp_event driver esp_http_client)
