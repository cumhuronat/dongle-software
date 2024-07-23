#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tusb.h"
#include "driver/gpio.h"
#include "autoreset.h"
#include "max3421_helpers.h"
#include "debug_helpers.h"

#define DEBUG_BUTTON_GPIO GPIO_NUM_0