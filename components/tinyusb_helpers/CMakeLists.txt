
idf_component_register(SRCS "src/usb_descriptors.c" "src/hcd_max3421.c" "src/dfu_helpers.c" "src/tusb_dfu_ota.c"
                       INCLUDE_DIRS "include"
                       REQUIRES "tinyusb" "helpers" "esp_partition" "app_update" "nvs_flash"
                       PRIV_REQUIRES "esp_driver_gpio" "esp_driver_spi")
