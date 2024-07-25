#include "main.h"

static uint8_t pcBuffer[64];
static uint8_t cncBuffer[64];
static int tudLastPoint = 0;
static int tuhLastPoint = 0;
static SemaphoreHandle_t xSemaphoreTUD = NULL;
static SemaphoreHandle_t xSemaphoreTUH = NULL;

void tuhTask(void *pvParameters)
{
    while (1)
    {
        tuh_task();
    }
}

void tudTask(void *pvParameters)
{
    while (1)
    {
        tud_task();
    }
}

void showSemaphoreStatus(void)
{
    BaseType_t tudSemCount = uxSemaphoreGetCount(xSemaphoreTUD);
    BaseType_t tuhSemCount = uxSemaphoreGetCount(xSemaphoreTUH);

    ESP_LOGI("SEMAPHORE_STATUS", "TUD Semaphore: %s (Count: %d)",
             tudSemCount > 0 ? "Available" : "Not Available",
             tudSemCount);

    ESP_LOGI("SEMAPHORE_STATUS", "TUH Semaphore: %s (Count: %d)",
             tuhSemCount > 0 ? "Available" : "Not Available",
             tuhSemCount);

    ESP_LOGI("SEMAPHORE_STATUS", "TUD Last Point: %d", tudLastPoint);
    ESP_LOGI("SEMAPHORE_STATUS", "TUH Last Point: %d", tuhLastPoint);

    ESP_LOGI("SEMAPHORE_STATUS", "tud_cdc_available: %u", (unsigned int)tud_cdc_available());
    ESP_LOGI("SEMAPHORE_STATUS", "tud_cdc_n_write_available: %u", (unsigned int)tud_cdc_n_write_available(0));
    ESP_LOGI("SEMAPHORE_STATUS", "tuh_cdc_read_available: %u", (unsigned int)tuh_cdc_read_available(0));
    ESP_LOGI("SEMAPHORE_STATUS", "tuh_cdc_write_available: %u", (unsigned int)tuh_cdc_write_available(0));



    //xSemaphoreGive(xSemaphoreTUD);
    //xSemaphoreGive(xSemaphoreTUH);

    //tud_cdc_read(pcBuffer, sizeof(pcBuffer));
    //tuh_cdc_read(0, cncBuffer, sizeof(cncBuffer));
}




void buttonTask(void *pvParameters)
{
    while (1)
    {
        if (gpio_get_level(DEBUG_BUTTON_GPIO) == 0)
        {
            setDebug(true);
            showSemaphoreStatus();
            //log_debug_data();
            //clearFrameInterrupt(1, 0);
            // vTaskDelete(NULL);
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void tudReadTask(void *pvParameters)
{
    while (1)
    {
        if (xSemaphoreTake(xSemaphoreTUD, portMAX_DELAY))
        {
            tudLastPoint = 1;
            size_t bytes;
            while (1)
            {
                tudLastPoint = 2;
                bytes = tud_cdc_read(pcBuffer, sizeof(pcBuffer));
                tudLastPoint = 3;
                if (bytes <= 0)
                    break;
                tudLastPoint = 4;
                tuh_cdc_write(0, pcBuffer, bytes);
                tudLastPoint = 5;
                tuh_cdc_write_flush(0);
                tudLastPoint = 6;
                taskYIELD();
                tudLastPoint = 7;
            }
        }
    }
}


void tuhReadTask(void *pvParameters)
{
    while (1)
    {
        if (xSemaphoreTake(xSemaphoreTUH, portMAX_DELAY))
        {
            tuhLastPoint = 1;
            size_t bytes;
            while (1)
            {
                tuhLastPoint = 2;
                bytes = tuh_cdc_read(0, cncBuffer, sizeof(cncBuffer));
                tuhLastPoint = 3;
                if (bytes <= 0)
                    break;
                tuhLastPoint = 4;
                tud_cdc_write(cncBuffer, bytes);
                tuhLastPoint = 5;
                tud_cdc_write_flush();
                tuhLastPoint = 6;
                taskYIELD();
                tuhLastPoint = 7;
            }
        }
    }
}

extern "C" void app_main(void)
{
    switch_to_tinyusb();
    tud_init(0);
    //tud_disconnect();
    tuh_init(1);

    xSemaphoreTUD = xSemaphoreCreateBinary();
    xSemaphoreTUH = xSemaphoreCreateBinary();

    xTaskCreatePinnedToCore(tuhTask, "tuhTask", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(tuhReadTask, "tuhReadTask", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(tudTask, "tudTask", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(tudReadTask, "tudReadTask", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(buttonTask, "buttonTask", 8192, NULL, 1, NULL, 1);
    //xTaskCreatePinnedToCore(refresher, "refresherTask", 4096, NULL, 1, NULL, 1);
}

extern "C"
{
    void tud_cdc_rx_cb(uint8_t itf)
    {
        xSemaphoreGive(xSemaphoreTUD);
    }

    void tud_cdc_tx_complete_cb(uint8_t itf)
    {
        xSemaphoreGive(xSemaphoreTUD);
    }

    void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const *line_coding)
    {
        if (line_coding->bit_rate == 123123)
        {
            switch_to_jtag();
        }
    }

    void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
    {
        ESP_LOGI("tud_cdc_line_state_cb", "DTR: %d, RTS: %d", dtr, rts);
        uint16_t const line_state = (dtr ? CDC_CONTROL_LINE_STATE_DTR : 0) |
                                    (rts ? CDC_CONTROL_LINE_STATE_RTS : 0);
        tuh_cdc_set_control_line_state(0, line_state, NULL, 0);
    }

    void tuh_cdc_mount_cb(uint8_t idx)
    {
        ESP_LOGI("tuh_cdc_mount_cb", "Mounting CDC %d", idx);
        tud_connect();
        tuh_cdc_set_baudrate(idx, 115200, NULL, 0);
    }

    void tuh_cdc_umount_cb(uint8_t idx)
    {
        ESP_LOGI("tuh_cdc_umount_cb", "Unmounting CDC %d", idx);
        tud_disconnect();
    }

    void tuh_cdc_rx_cb(uint8_t idx)
    {
        xSemaphoreGive(xSemaphoreTUH);
    }

    void tuh_cdc_tx_complete_cb(uint8_t idx)
    {
        xSemaphoreGive(xSemaphoreTUH);
    }
}