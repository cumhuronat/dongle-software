
idf_component_register(SRCS "src/main.c"
                       PRIV_INCLUDE_DIRS "include"
                       PRIV_REQUIRES "esp_driver_gpio" "esp_driver_uart" "usb" "usb_host_cdc_acm" "autoreset" "helpers" "ble_manager" "esp_timer")
