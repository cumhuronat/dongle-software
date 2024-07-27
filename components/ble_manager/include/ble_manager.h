#pragma once
#include "macros.h"
#ifdef __cplusplus
#include <NimBLEDevice.h>
#include <cstdio> 
#include "tuh_stream_processor.h"
#include <map>

class BLEManager {
public:
    BLEManager();
    void init();
};

void ble_manager_task(void *pvParameters);
#endif

EXTERN_C_BEGIN
    extern QueueHandle_t writeCommandQueue;
    void ble_manager_init();
EXTERN_C_END