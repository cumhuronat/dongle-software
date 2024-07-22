set(srcs descriptors.c)
idf_component_register(SRCS "${srcs}" REQUIRES usb driver tinyusb)