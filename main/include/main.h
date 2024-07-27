#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tusb.h"
#include "driver/gpio.h"
#include "autoreset.h"
#include "debug_helpers.h"
#include "tuh_stream_processor.h"
#include "ble_manager.h"

#define DEBUG_BUTTON_GPIO GPIO_NUM_0
