
idf_component_register(SRCS "src/main.cpp"
                       PRIV_INCLUDE_DIRS "include"
                       REQUIRES "tinyusb" "autoreset" "helpers"
                       PRIV_REQUIRES "esp_driver_gpio")
