#include "main.h"

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

void buttonTask(void *pvParameters)
{
    while (1)
    {
        if (gpio_get_level(DEBUG_BUTTON_GPIO) == 0)
        {
            setDebug(true);
            vTaskDelete(NULL);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
void writeCommandReadTask(void *pvParameters)
{
    Command command;
    while (1)
    {
        if (xQueueReceive(writeCommandQueue, &command, portMAX_DELAY))
        {

            tuh_cdc_write(0, command.command, strlen(command.command));
            tuh_cdc_write(0, "\n", 1);
            tuh_cdc_write_flush(0);
        }
    }
}

void tudReadTask(void *pvParameters)
{
    static uint8_t pcBuffer[64];
    while (1)
    {
        if (xSemaphoreTake(xSemaphoreTUD, portMAX_DELAY))
        {
            size_t bytes;
            while (1)
            {
                bytes = tud_cdc_read(pcBuffer, sizeof(pcBuffer));
                if (bytes <= 0)
                    break;
                tuh_cdc_write(0, pcBuffer, bytes);
                tuh_cdc_write_flush(0);
                taskYIELD();
            }
        }
    }
}

void tuhReadTask(void *pvParameters)
{
    static uint8_t cncBuffer[64];
    while (1)
    {
        if (xSemaphoreTake(xSemaphoreTUH, portMAX_DELAY))
        {
            size_t bytes;
            while (1)
            {
                bytes = tuh_cdc_read(0, cncBuffer, sizeof(cncBuffer));
                if (bytes <= 0)
                    break;
                tud_cdc_write(cncBuffer, bytes);
                tud_cdc_write_flush();
                process_received_data(cncBuffer, bytes);
                taskYIELD();
            }
        }
    }
}

int cdc_vprintf(const char *format, va_list args)
{
    char buffer[256];
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    if (len > 0)
    {
        if (tud_cdc_n_connected(1))
        {
            tud_cdc_n_write(1, buffer, len);
            tud_cdc_n_write_flush(1);
        }
    }
    return len;
}

void app_main(void)
{
    esp_log_set_vprintf(cdc_vprintf);
    switch_to_tinyusb();
    tusb_init();
    tud_disconnect();
    stream_processor_init();
    ble_manager_init();

    xSemaphoreTUD = xSemaphoreCreateBinary();
    xSemaphoreTUH = xSemaphoreCreateBinary();

    xTaskCreatePinnedToCore(tuhTask, "tuhTask", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(tuhReadTask, "tuhReadTask", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(tudTask, "tudTask", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(tudReadTask, "tudReadTask", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(buttonTask, "buttonTask", 8192, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(writeCommandReadTask, "writeCommandReadTask", 4096, NULL, 1, NULL, 1);
}

void tud_cdc_rx_cb(uint8_t itf)
{
    if (itf == 0) xSemaphoreGive(xSemaphoreTUD);
}

void tud_cdc_tx_complete_cb(uint8_t itf)
{
    if (itf == 0) xSemaphoreGive(xSemaphoreTUD);
}

void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const *line_coding)
{
    if (line_coding->bit_rate == 123123)
    {
        tud_deinit(0);
        tuh_deinit(1);
        switch_to_jtag();
    }
}

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    if (itf == 0)
    {
        uint16_t const line_state = (dtr ? CDC_CONTROL_LINE_STATE_DTR : 0) |
                                    (rts ? CDC_CONTROL_LINE_STATE_RTS : 0);
        tuh_cdc_set_control_line_state(0, line_state, NULL, 0);
    }
}

void tuh_cdc_mount_cb(uint8_t idx)
{
    ESP_LOGW("CDC_TASK", "Mounting CDC interface");
    tud_connect();
    tuh_cdc_set_baudrate(idx, 115200, NULL, 0);
}

void tuh_cdc_umount_cb(uint8_t idx)
{
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