#pragma once

#include "macros.h"
#include <stdlib.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <string.h>

// Define separate structs for each property
typedef struct {
    char status[10];
} StatusUpdate;

typedef struct {
    float x, y, z;
} WPosUpdate;

typedef struct {
    float feedRate, spindleSpeed;
} FSUpdate;

typedef struct {
    bool x, y1, y2, z, h;
} PinUpdate;

typedef struct {
    int queueItems;
} QueueItemsUpdate;

typedef struct {
    float x, y, z;
} WCOUpdate;

typedef struct {
    int feed, rapid, spindle;
} OvUpdate;


typedef enum {
    STATUS_UPDATE,
    WPOS_UPDATE,
    FS_UPDATE,
    PIN_UPDATE,
    QUEUE_ITEMS_UPDATE,
    WCO_UPDATE,
    OV_UPDATE
} UpdateType;

typedef struct {
    UpdateType type;
    union {
        StatusUpdate statusUpdate;
        WPosUpdate wposUpdate;
        FSUpdate fsUpdate;
        PinUpdate pinUpdate;
        QueueItemsUpdate queueItemsUpdate;
        WCOUpdate wcoUpdate;
        OvUpdate ovUpdate;
    } data;
} UpdateMessage;

extern QueueHandle_t xQueueUpdates;


void parse_status(char *statusString);
void stream_processor_init(void);
void process_received_data(const uint8_t *buffer, size_t length);