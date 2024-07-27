
idf_component_register(SRCS "src/main.c"
                       PRIV_INCLUDE_DIRS "include"
                       REQUIRES "tinyusb" "autoreset" "helpers" "ble_manager"
                       PRIV_REQUIRES "esp_driver_gpio")
