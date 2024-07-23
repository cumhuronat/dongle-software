#include "main.h"

static uint8_t pcBuffer[64];
static uint8_t cncBuffer[64];

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

extern "C" void app_main(void)
{
    switch_to_tinyusb();
    max3421_init();
    tuh_init(1);

    xTaskCreatePinnedToCore(tuhTask, "tuhTask", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(tudTask, "tudTask", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(buttonTask, "buttonTask", 2048, NULL, 5, NULL, 1);
}

extern "C"
{
    void tud_cdc_rx_cb(uint8_t itf)
    {
        debugLog("tudc\r\n");
        uint32_t count;
        while (1)
        {
            count = tud_cdc_read(pcBuffer, 64);
            if (count <= 0)
                break;
            if (!tuh_cdc_write_available(0))
                return;
            tuh_cdc_write(0, pcBuffer, count);
            taskYIELD();
        }
        tuh_cdc_write_flush(0);
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
        uint16_t const line_state = (dtr ? CDC_CONTROL_LINE_STATE_DTR : 0) |
                                    (rts ? CDC_CONTROL_LINE_STATE_RTS : 0);
        tuh_cdc_set_control_line_state(0, line_state, NULL, 0);
    }

    void tuh_cdc_mount_cb(uint8_t idx)
    {
        tud_init(0);
        tud_connect();
        tuh_cdc_set_baudrate(idx, 115200, NULL, 0);
    }

    void tuh_cdc_umount_cb(uint8_t idx)
    {
        tud_disconnect();
    }

    void tuh_cdc_rx_cb(uint8_t idx)
    {
        size_t bytes;
        while (1)
        {
            bytes = tuh_cdc_read(idx, cncBuffer, 64);
            if (bytes <= 0)
            {
                break;
            }
            if (!tud_cdc_write_available() || !tud_cdc_connected())
                return;
            tud_cdc_write(cncBuffer, bytes);
            taskYIELD();
        }
        tud_cdc_write_flush();
    }
}