
idf_component_register(SRCS "src/autoreset.c"
                       INCLUDE_DIRS "include"
                       REQUIRES "usb" "helpers"
                       PRIV_REQUIRES "driver")
