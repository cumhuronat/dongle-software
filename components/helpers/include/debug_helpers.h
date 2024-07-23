#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include "macros.h"
#include "esp_attr.h"

EXTERN_C_BEGIN
void debugLog(const char *format, ...);
void setDebug(bool on);
EXTERN_C_END