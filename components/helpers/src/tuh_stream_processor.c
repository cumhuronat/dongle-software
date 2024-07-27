#include "tuh_stream_processor.h"
#include "grbl_status_parser.h"


static char *TAG = "tuh_processor";

StatusUpdate currentStatusUpdate;
WPosUpdate currentWPosUpdate;
FSUpdate currentFSUpdate;
PinUpdate currentPinUpdate;
QueueItemsUpdate currentQueueItemsUpdate;
WCOUpdate currentWCOUpdate;
OvUpdate currentOvUpdate;
QueueHandle_t xQueueUpdates;



static char *get_value(const char *data, char separator, int index)
{
    int found = 0;
    int startIndex = 0;
    int endIndex = -1;
    int maxIndex = strlen(data) - 1;
    static char buffer[32]; // Assumes no value exceeds 32 characters

    for (int i = 0; i <= maxIndex && found <= index; i++)
    {
        if (data[i] == separator || i == maxIndex)
        {
            found++;
            startIndex = endIndex + 1;
            endIndex = (i == maxIndex) ? i + 1 : i;
        }
    }

    if (found > index)
    {
        strncpy(buffer, data + startIndex, endIndex - startIndex);
        buffer[endIndex - startIndex] = '\0';
        return buffer;
    }

    return "";
}

void stream_processor_init(void)
{
    xQueueUpdates = xQueueCreate(10, sizeof(UpdateMessage));
}

void process_received_data(uint8_t *buffer, size_t length)
{
    static char lineBuffer[128];
    static size_t lineBufferIndex = 0;

    for (size_t i = 0; i < length; i++)
    {
        if (buffer[i] == '\n' || lineBufferIndex >= sizeof(lineBuffer) - 1)
        {
            lineBuffer[lineBufferIndex] = '\0';
            if (lineBuffer[0] == '<')
            {
                parse_status(lineBuffer);
            }

            lineBufferIndex = 0;
        }
        else
        {
            lineBuffer[lineBufferIndex++] = buffer[i];
        }
    }
}
void parse_status(char *statusString) {
    char *segment;
    char *segments[10];  // Assuming maximum 10 segments
    int segmentCount = 0;

    char statusStringCopy[128];
    strncpy(statusStringCopy, statusString, sizeof(statusStringCopy) - 1);
    statusStringCopy[sizeof(statusStringCopy) - 1] = '\0';

    char *statusStringPtr = statusStringCopy + 1;  // Remove the starting '<'
    statusStringPtr[strlen(statusStringPtr) - 1] = '\0';  // Remove the ending '>'

    segment = strtok(statusStringPtr, "|");
    while (segment != NULL && segmentCount < 10) {
        segments[segmentCount++] = segment;
        segment = strtok(NULL, "|");
    }

    for (int i = 0; i < segmentCount; i++) {
        segment = segments[i];
        UpdateMessage updateMessage;

        if (i == 0) {
            StatusUpdate statusUpdate = {};
            strncpy(statusUpdate.status, segment, sizeof(statusUpdate.status) - 1);
            if (strcmp(statusUpdate.status, currentStatusUpdate.status) != 0) {
                updateMessage.type = STATUS_UPDATE;
                updateMessage.data.statusUpdate = statusUpdate;
                xQueueSend(xQueueUpdates, &updateMessage, 0);
                strncpy(currentStatusUpdate.status, statusUpdate.status, sizeof(currentStatusUpdate.status) - 1);
                ESP_LOGI(TAG, "Status: %s", statusUpdate.status);
            }
        } else if (strncmp(segment, "WPos:", 5) == 0) {
            WPosUpdate wposUpdate = {};
            wposUpdate.x = atof(get_value(segment + 5, ',', 0));
            wposUpdate.y = atof(get_value(segment + 5, ',', 1));
            wposUpdate.z = atof(get_value(segment + 5, ',', 2));
            if (wposUpdate.x != currentWPosUpdate.x || wposUpdate.y != currentWPosUpdate.y || wposUpdate.z != currentWPosUpdate.z) {
                updateMessage.type = WPOS_UPDATE;
                updateMessage.data.wposUpdate = wposUpdate;
                xQueueSend(xQueueUpdates, &updateMessage, 0);
                currentWPosUpdate = wposUpdate;
                ESP_LOGI(TAG, "WPos: X=%.3f Y=%.3f Z=%.3f", wposUpdate.x, wposUpdate.y, wposUpdate.z);
            }
        } else if (strncmp(segment, "FS:", 3) == 0) {
            FSUpdate fsUpdate = {};
            fsUpdate.feedRate = atof(get_value(segment + 3, ',', 0));
            fsUpdate.spindleSpeed = atof(get_value(segment + 3, ',', 1));
            if (fsUpdate.feedRate != currentFSUpdate.feedRate || fsUpdate.spindleSpeed != currentFSUpdate.spindleSpeed) {
                updateMessage.type = FS_UPDATE;
                updateMessage.data.fsUpdate = fsUpdate;
                xQueueSend(xQueueUpdates, &updateMessage, 0);
                currentFSUpdate = fsUpdate;
                ESP_LOGI(TAG, "FS: Feed=%.3f Speed=%.3f", fsUpdate.feedRate, fsUpdate.spindleSpeed);
            }
        } else if (strncmp(segment, "Pn:", 3) == 0) {
            PinUpdate pinUpdate = {};
            pinUpdate.x = strstr(segment, "X") != NULL;
            pinUpdate.y1 = strstr(segment, "Y1") != NULL;
            pinUpdate.y2 = strstr(segment, "Y2") != NULL;
            pinUpdate.z = strstr(segment, "Z") != NULL;
            pinUpdate.h = strstr(segment, "H") != NULL;
            if (pinUpdate.x != currentPinUpdate.x || pinUpdate.y1 != currentPinUpdate.y1 || pinUpdate.y2 != currentPinUpdate.y2 || pinUpdate.z != currentPinUpdate.z || pinUpdate.h != currentPinUpdate.h) {
                updateMessage.type = PIN_UPDATE;
                updateMessage.data.pinUpdate = pinUpdate;
                xQueueSend(xQueueUpdates, &updateMessage, 0);
                currentPinUpdate = pinUpdate;
                ESP_LOGI(TAG, "Pin: X=%d Y1=%d Y2=%d Z=%d H=%d", pinUpdate.x, pinUpdate.y1, pinUpdate.y2, pinUpdate.z, pinUpdate.h);
            }
        } else if (strncmp(segment, "Q:", 2) == 0) {
            QueueItemsUpdate queueItemsUpdate = {};
            queueItemsUpdate.queueItems = atoi(segment + 2);
            if (queueItemsUpdate.queueItems != currentQueueItemsUpdate.queueItems) {
                updateMessage.type = QUEUE_ITEMS_UPDATE;
                updateMessage.data.queueItemsUpdate = queueItemsUpdate;
                xQueueSend(xQueueUpdates, &updateMessage, 0);
                currentQueueItemsUpdate.queueItems = queueItemsUpdate.queueItems;
                ESP_LOGI(TAG, "QueueItems: %d", queueItemsUpdate.queueItems);
            }
        } else if (strncmp(segment, "WCO:", 4) == 0) {
            WCOUpdate wcoUpdate = {};
            wcoUpdate.x = atof(get_value(segment + 4, ',', 0));
            wcoUpdate.y = atof(get_value(segment + 4, ',', 1));
            wcoUpdate.z = atof(get_value(segment + 4, ',', 2));
            if (wcoUpdate.x != currentWCOUpdate.x || wcoUpdate.y != currentWCOUpdate.y || wcoUpdate.z != currentWCOUpdate.z) {
                updateMessage.type = WCO_UPDATE;
                updateMessage.data.wcoUpdate = wcoUpdate;
                xQueueSend(xQueueUpdates, &updateMessage, 0);
                currentWCOUpdate = wcoUpdate;
                ESP_LOGI(TAG, "WCO: X=%.3f Y=%.3f Z=%.3f", wcoUpdate.x, wcoUpdate.y, wcoUpdate.z);
            }
        } else if (strncmp(segment, "Ov:", 3) == 0) {
            OvUpdate ovUpdate = {};
            ovUpdate.feed = atoi(get_value(segment + 3, ',', 0));
            ovUpdate.rapid = atoi(get_value(segment + 3, ',', 1));
            ovUpdate.spindle = atoi(get_value(segment + 3, ',', 2));
            if (ovUpdate.feed != currentOvUpdate.feed || ovUpdate.rapid != currentOvUpdate.rapid || ovUpdate.spindle != currentOvUpdate.spindle) {
                updateMessage.type = OV_UPDATE;
                updateMessage.data.ovUpdate = ovUpdate;
                xQueueSend(xQueueUpdates, &updateMessage, 0);
                currentOvUpdate = ovUpdate;
                ESP_LOGI(TAG, "Ov: Feed=%d Rapid=%d Spindle=%d", ovUpdate.feed, ovUpdate.rapid, ovUpdate.spindle);
            }
        }
    }
}