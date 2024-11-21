#include "main.h"

static const char *TAG = "Main";
static cdc_acm_dev_hdl_t cdc_dev = NULL;
static QueueHandle_t uart0_queue;
#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)

static uint16_t cnc_vid;
static uint16_t cnc_pid;
static int64_t last_packet_time = -999999999;
static bool handle_rx(const uint8_t *data, size_t data_len, void *arg)
{
    ESP_LOGD(TAG, "Received %d bytes from UART", data_len);
    uart_write_bytes(0, data, data_len);
    uart_wait_tx_done(0, portMAX_DELAY);
    ESP_LOGD(TAG, "Sent %d bytes to CDC-ACM", data_len);
    process_received_data(data, data_len);
    ESP_LOGD(TAG, "Data processed");
    return true;
}

static void uart_event_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t *dtmp = (uint8_t *)malloc(RD_BUF_SIZE);
    while (1)
    {
        if (xQueueReceive(uart0_queue, (void *)&event, portMAX_DELAY))
        {
            if (event.type == UART_DATA)
            {
                last_packet_time = esp_timer_get_time();
                int bytes = uart_read_bytes(0, dtmp, event.size, portMAX_DELAY);
                if (cdc_dev != NULL && bytes > 0)
                    cdc_acm_host_data_tx_blocking(cdc_dev, dtmp, event.size, portMAX_DELAY);
            }
            else if (event.type == UART_FIFO_OVF)
            {
                ESP_LOGI(TAG, "RX FIFO overflow detected");
            }
            else if (event.type == UART_BUFFER_FULL)
            {
                ESP_LOGI(TAG, "RX buffer full");
            }
            else if (event.type == UART_BREAK)
            {
                ESP_LOGI(TAG, "Break detected");
            }
            else if (event.type == UART_PARITY_ERR)
            {
                ESP_LOGI(TAG, "Parity error detected");
            }
            else if (event.type == UART_FRAME_ERR)
            {
                ESP_LOGI(TAG, "Frame error detected");
            }
            else if (event.type == UART_EVENT_MAX)
            {
                ESP_LOGI(TAG, "Max event detected");
            }
        }
    }
}

static void handle_event(const cdc_acm_host_dev_event_data_t *event, void *user_ctx)
{
    switch (event->type)
    {
    case CDC_ACM_HOST_ERROR:
        ESP_LOGE(TAG, "CDC-ACM error has occurred, err_no = %i", event->data.error);
        break;
    case CDC_ACM_HOST_DEVICE_DISCONNECTED:
        ESP_LOGI(TAG, "Device suddenly disconnected");
        ESP_ERROR_CHECK(cdc_acm_host_close(event->data.cdc_hdl));
        break;
    case CDC_ACM_HOST_SERIAL_STATE:
        ESP_LOGI(TAG, "Serial state notif 0x%04X", event->data.serial_state.val);
        break;
    case CDC_ACM_HOST_NETWORK_CONNECTION:
    default:
        ESP_LOGW(TAG, "Unsupported CDC event: %i", event->type);
        break;
    }
}

static void usb_lib_task(void *arg)
{
    while (1)
    {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
    }
}

void timerTask(void *pvParameters)
{
    while (1)
    {
        if (gpio_get_level(DEBUG_BUTTON_GPIO) == 0)
        {
            esp_log_level_set("*", ESP_LOG_VERBOSE);
            uart_event_t event;
            uint8_t *dtmp = (uint8_t *)malloc(RD_BUF_SIZE);
            if (xQueueReceive(uart0_queue, (void *)&event, (TickType_t)1))
            {
                if (event.type == UART_DATA)
                {
                    ESP_LOGI(TAG, "Received %d bytes from UART", event.size);
                    int bytes = uart_read_bytes(0, dtmp, event.size, 1);
                    ESP_LOGI(TAG, "Sent %d bytes to CDC-ACM", bytes);
                    cdc_acm_host_data_tx_blocking(cdc_dev, dtmp, bytes, 200);
                    ESP_LOGI(TAG, "Data processed");
                }
                else
                {
                    ESP_LOGE(TAG, "Unexpected event type: %i", event.type);
                }
            }
            else
            {
                ESP_LOGE(TAG, "Failed to receive event from queue");
            }
            free(dtmp);
        }
        if (cdc_dev != NULL && esp_timer_get_time() - last_packet_time > 2000000)
        {
            ESP_LOGI(TAG, "No packets received for more than 2 seconds, closing CDC-ACM connection");
            cdc_acm_host_close(cdc_dev);
            cdc_dev = NULL;
        }
        else if (cdc_dev == NULL && (last_packet_time > esp_timer_get_time() - 2000000))
        {
            ESP_LOGI(TAG, "Attempting to reconnect");
            cdc_acm_host_device_config_t dev_config = {
                .connection_timeout_ms = 5000,
                .out_buffer_size = 2048,
                .in_buffer_size = 2048,
                .event_cb = handle_event,
                .data_cb = handle_rx};
            cdc_acm_host_open(0x16D0, 0x103E, 0, &dev_config, &cdc_dev);

            ESP_ERROR_CHECK(cdc_acm_host_set_control_line_state(cdc_dev, true, true));

            cdc_acm_line_coding_t line_coding = {
                .dwDTERate = 115200,
                .bDataBits = 8,
                .bParityType = 0,
                .bCharFormat = 0,
            };

            ESP_ERROR_CHECK(cdc_acm_host_line_coding_set(cdc_dev, &line_coding));
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void writeCommandReadTask(void *pvParameters)
{
    char *command;
    while (1)
    {
        if (xQueueReceive(writeCommandQueue, &command, portMAX_DELAY))
        {
            ESP_ERROR_CHECK(cdc_acm_host_data_tx_blocking(cdc_dev, (uint8_t *)command, strlen(command), 100));
            free(command);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void new_dev_cb(usb_device_handle_t usb_dev)
{
    const usb_device_desc_t *device_desc;
    usb_host_get_device_descriptor(usb_dev, &device_desc);
    cnc_vid = device_desc->idVendor;
    cnc_pid = device_desc->idProduct;
}

void app_main(void)
{
    stream_processor_init();
    ble_manager_init();

    uart_config_t uart_config = {
        .baud_rate = 115385,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_driver_install(0, BUF_SIZE * 2, BUF_SIZE * 2, 3, &uart0_queue, 0);
    uart_param_config(0, &uart_config);
    uart_set_pin(0, 43, 44, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;        // Disable interrupt
    io_conf.mode = GPIO_MODE_OUTPUT;              // Set as output mode
    io_conf.pin_bit_mask = 1ULL << 42;            // Bit mask of the pin to set
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE; // Disable pull-down mode
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;     // Disable pull-up mode
    gpio_config(&io_conf);

    gpio_set_level(42, 1);

    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    usb_host_install(&host_config);

    const cdc_acm_host_driver_config_t driver_config = {
        .driver_task_priority = 11,
        .driver_task_stack_size = 2048,
        .xCoreID = 0,
        .new_dev_cb = new_dev_cb,
    };
    cdc_acm_host_install(&driver_config);

    xTaskCreatePinnedToCore(usb_lib_task, "usb_lib", 4096, xTaskGetCurrentTaskHandle(), 10, NULL, 0);

    xTaskCreatePinnedToCore(uart_event_task, "uart_event_task", 2048, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(timerTask, "timerTask", 8192, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(writeCommandReadTask, "writeCommandReadTask", 4096, NULL, 1, NULL, 1);
}
