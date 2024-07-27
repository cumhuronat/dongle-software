#include "debug_helpers.h"

static bool debugOn = false;

void IRAM_ATTR debugLog(const char *format, ...)
{
    if (debugOn)
    {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
}

void setDebug(bool on)
{
    debugOn = on;
    debugLog("Debug %s\n", debugOn ? "enabled" : "disabled");
}