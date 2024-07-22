set(srcs main.cpp hcd_max3421.c tusb_esp32.c descriptors.c autoreset.c)
set(incdirs ".")
idf_component_register(SRCS "main.cpp" "${srcs}"
                        INCLUDE_DIRS "${incdirs}"
                        REQUIRES tinyusb usb driver)

                        
idf_component_get_property(tusb_lib tinyusb COMPONENT_LIB)
target_include_directories(${tusb_lib} PUBLIC "${COMPONENT_DIR}")
