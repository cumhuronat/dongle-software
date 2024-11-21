#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "autoreset.h"
#include "debug_helpers.h"
#include "tuh_stream_processor.h"
#include "ble_manager.h"
#include "usb/usb_host.h"
#include "usb/cdc_acm_host.h"
#include "esp_timer.h"

#define DEBUG_BUTTON_GPIO GPIO_NUM_0
