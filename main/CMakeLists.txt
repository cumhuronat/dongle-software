set(srcs main.cpp Adafruit_USBH_Host.cpp hcd_max3421.c)
set(incdirs ".")
idf_component_register(SRCS "${srcs}"
                        INCLUDE_DIRS "${incdirs}"
                        REQUIRES usb driver)

                        
idf_component_get_property(tusb_lib tinyusb COMPONENT_LIB)
target_include_directories(${tusb_lib} PUBLIC "${COMPONENT_DIR}")
