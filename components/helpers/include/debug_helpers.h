#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include "macros.h"
#include "esp_attr.h"
#include "stdbool.h"

void debugLog(const char *format, ...);
void setDebug(bool on);