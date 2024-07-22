#pragma once

#include "sdkconfig.h"
#include <stdlib.h>
#include <stdbool.h>
#include "esp_log.h"
#include "soc/soc.h"
#include "soc/efuse_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/usb_struct.h"
#include "soc/usb_reg.h"
#include "soc/usb_wrap_reg.h"
#include "soc/usb_wrap_struct.h"
#include "soc/usb_periph.h"
#include "soc/periph_defs.h"
#include "soc/timer_group_struct.h"
#include "soc/system_reg.h"
#include "rom/gpio.h"

#include "hal/gpio_ll.h"
#include "hal/clk_gate_ll.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "driver/gpio.h"
#include "hal/usb_serial_jtag_ll.h"
#include "esp32s3/rom/usb/usb_persist.h"
#include "esp32s3/rom/usb/usb_dc.h"
#include "esp32s3/rom/usb/chip_usb_dw_wrapper.h"
#include "esp_private/usb_phy.h"
#include "hal/usb_fsls_phy_ll.h"
#include "esp_log.h"


#ifdef __cplusplus
extern "C" {
#endif


void usb_switch_to_cdc_jtag(usb_phy_handle_t phy_handle);
void usb_persist_restart(usb_phy_handle_t phy_handle);

#ifdef __cplusplus
}
#endif