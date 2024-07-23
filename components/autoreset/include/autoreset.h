#pragma once

#include "macros.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_private/usb_phy.h"
#include "soc/rtc_cntl_reg.h"


EXTERN_C_BEGIN

void switch_to_jtag(void);
void switch_to_tinyusb(void);

EXTERN_C_END