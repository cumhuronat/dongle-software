
idf_component_register(SRCS "src/usb_descriptors.c" "src/max3421_helpers.c" "src/hcd_max3421.c" "src/dfu_helpers.c" "src/tusb_dfu_ota.c"
                       INCLUDE_DIRS "include"
                       REQUIRES "tinyusb" "helpers" "esp_partition" "app_update" "nvs_flash"
                       PRIV_REQUIRES "driver")
