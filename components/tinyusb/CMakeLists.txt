idf_build_get_property(target IDF_TARGET)

if(target STREQUAL "esp32s3")
    set(tusb_mcu "OPT_MCU_ESP32S3")
    set(tusb_family "esp32sx")
elseif(target STREQUAL "esp32s2")
    set(tusb_mcu "OPT_MCU_ESP32S2")
    set(tusb_family "esp32sx")
elseif(target STREQUAL "esp32p4")
    set(tusb_mcu "OPT_MCU_ESP32P4")
    set(tusb_family "esp32px")
endif()

set(compile_options
    "-DCFG_TUSB_MCU=${tusb_mcu}"
    )

idf_component_get_property(freertos_include freertos ORIG_INCLUDE_PATH)

set(includes_private
    "src/"
    "src/device"
    "src/host" # For RNDIS definitions
    )

set(includes_public
    "src/"
    # The FreeRTOS API include convention in tinyusb is different from esp-idf
    "${freertos_include}"
    )

set(srcs
    "src/portable/espressif/esp32sx/dcd_esp32sx.c"
    "src/host/usbh.c"
    "src/class/hid/hid_host.c"
    "src/class/cdc/cdc_host.c"
    "src/class/msc/msc_host.c"
    "src/host/hub.c"
    "src/class/cdc/cdc_device.c"
    "src/common/tusb_fifo.c"
    "src/device/usbd_control.c"
    "src/device/usbd.c"
    "src/tusb.c"
    )
    
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${includes_public}
                       PRIV_INCLUDE_DIRS ${includes_private}
                       REQUIRES main
                       )

target_compile_options(${COMPONENT_LIB} PUBLIC ${compile_options})

# when no builtin class driver is enabled, an uint8_t data compared with `BUILTIN_DRIVER_COUNT` will always be false
set_source_files_properties("src/device/usbd.c" PROPERTIES COMPILE_FLAGS "-Wno-type-limits")
