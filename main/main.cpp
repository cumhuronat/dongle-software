#include "tusb_config.h"
#include <stdint.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tusb.h"
#include "sdkconfig.h"
#include "esp_private/periph_ctrl.h"
#include "esp_private/usb_phy.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "autoreset.h"

#define DEBUG_BUTTON_GPIO GPIO_NUM_0
#define CDC_LINE_IDLE 0
#define CDC_LINE_1 1
#define CDC_LINE_2 2
#define CDC_LINE_3 3

static const char *TAG = "example";
static uint8_t buf[64];
static uint8_t bufh[64];
static bool debugOn = false;
static uint8_t lineState = 0;

static usb_phy_handle_t phy_hdl;

extern "C" int log_printf(const char *format, ...)
{
    if (!debugOn)
    {
        return 0;
    }
    va_list args;
    va_start(args, format);
    char buffer[256]; // Adjust size as needed
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    ESP_LOGI("TINY", "%s", buffer);
    return 0;
}

extern "C" void debugLog(const char *format, ...)
{
    if (debugOn)
    {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
}

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
            debugOn = true;
            ESP_LOGI(TAG, "Debug mode: %s", debugOn ? "ON" : "OFF");
            vTaskDelete(NULL);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

extern "C"
{
    void max3421_init(void);
}

extern "C" void app_main(void)
{
    usb_phy_config_t phy_conf = {
        .controller = USB_PHY_CTRL_OTG,
        .target = USB_PHY_TARGET_INT,
        .otg_mode = USB_OTG_MODE_DEVICE,
    };

    usb_new_phy(&phy_conf, &phy_hdl);

    max3421_init();
    tusb_init();

    xTaskCreatePinnedToCore(tuhTask, "tuhTask", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(tudTask, "tudTask", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(buttonTask, "buttonTask", 2048, NULL, 5, NULL, 1);
}

extern "C"
{
    void tud_cdc_rx_cb(uint8_t itf)
    {
        uint32_t count;
        while (1)
        {
            count = tud_cdc_read(buf, 64);
            if (count <= 0)
                break;
            tuh_cdc_write(0, buf, count);
        }
        tuh_cdc_write_flush(0);
    }

    void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const *line_coding)
    {
        if (line_coding->bit_rate == 123123)
        {
            ESP_LOGI(TAG, "Restarting in bootloader mode");
            usb_persist_restart(phy_hdl);
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
        tuh_cdc_set_control_line_state(idx, tud_cdc_get_line_state(), NULL, 0);
        tuh_cdc_set_baudrate(idx, 115200, NULL, 0);
    }

    void tuh_cdc_umount_cb(uint8_t idx)
    {
        ESP_LOGI(TAG, "CDC unmounted");
    }

    void tuh_cdc_rx_cb(uint8_t idx)
    {
        size_t bytes;
        while (1)
        {
            bytes = tuh_cdc_read(idx, bufh, 64);
            if (bytes <= 0)
            {
                break;
            }
            if (tud_cdc_connected())
                tud_cdc_write(bufh, bytes);
        }
        if (tud_cdc_connected())
            tud_cdc_write_flush();
    }
}