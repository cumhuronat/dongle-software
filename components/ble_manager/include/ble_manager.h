#pragma once
#include "macros.h"
#ifdef __cplusplus
#include <NimBLEDevice.h>
#include <cstdio> 
#include "tuh_stream_processor.h"
#include <map>

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_STATUS_UUID "00000001-0000-1000-8000-00805f9b34fb"
#define CHARACTERISTIC_WPOS_UUID "00000002-0000-1000-8000-00805f9b34fb"
#define CHARACTERISTIC_FS_UUID "00000003-0000-1000-8000-00805f9b34fb"
#define CHARACTERISTIC_PIN_UUID "00000004-0000-1000-8000-00805f9b34fb"
#define CHARACTERISTIC_QUEUE_UUID "00000005-0000-1000-8000-00805f9b34fb"
#define CHARACTERISTIC_WCO_UUID "00000006-0000-1000-8000-00805f9b34fb"
#define CHARACTERISTIC_OV_UUID "00000007-0000-1000-8000-00805f9b34fb"
#define CHARACTERISTIC_WRITE_UUID "00000008-0000-1000-8000-00805f9b34fb"

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