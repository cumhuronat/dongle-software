
#include "Adafruit_USBH_Host.h"
#include "esp_log.h"

Adafruit_USBH_Host *Adafruit_USBH_Host::_instance = NULL;

Adafruit_USBH_Host::Adafruit_USBH_Host(void)
{
    Adafruit_USBH_Host::_instance = this;
    _rhport = 0;
    _spi = NULL;
}

extern "C" uint8_t tuh_max3421_reg_read(uint8_t rhport, uint8_t reg,
                                        bool in_isr);

extern "C" bool tuh_max3421_reg_write(uint8_t rhport, uint8_t reg, uint8_t data,
                                      bool in_isr);

static void max3421_isr(void *arg);

SemaphoreHandle_t max3421_intr_sem;
static void max3421_intr_task(void *param);

uint8_t Adafruit_USBH_Host::max3421_readRegister(uint8_t reg, bool in_isr)
{
    return tuh_max3421_reg_read(_rhport, reg, in_isr);
}

bool Adafruit_USBH_Host::max3421_writeRegister(uint8_t reg, uint8_t data,
                                               bool in_isr)
{
    return tuh_max3421_reg_write(_rhport, reg, data, in_isr);
}

bool Adafruit_USBH_Host::configure(uint8_t rhport, uint32_t cfg_id,
                                   const void *cfg_param)
{
    return tuh_configure(rhport, cfg_id, cfg_param);
}

void Adafruit_USBH_Host::task(uint32_t timeout_ms, bool in_isr)
{
    tuh_task_ext(timeout_ms, in_isr);
}

bool Adafruit_USBH_Host::begin(uint8_t rhport)
{

    esp_err_t ret;
    spi_bus_config_t buscfg = {
        .mosi_io_num = GPIO_NUM_35,
        .miso_io_num = GPIO_NUM_37,
        .sclk_io_num = GPIO_NUM_36,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4092};
    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = 26 * 1000 * 1000,
        .spics_io_num = -1,
        .queue_size = 7,
    };
    // Initialize the SPI bus
    ret = spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    ret = spi_bus_add_device(SPI3_HOST, &devcfg, &_spi);
    ESP_ERROR_CHECK(ret);

    // Create an task for executing interrupt handler in thread mode
    max3421_intr_sem = xSemaphoreCreateBinary();
    xTaskCreatePinnedToCore(max3421_intr_task, "max3421 intr", 4096, NULL, 5, NULL, 1);

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << GPIO_NUM_10);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    // Initialize CS pin to inactive state
    gpio_set_level(GPIO_NUM_10, 1);

    // Configure the GPIO as input with pull-up
    gpio_config_t io2_conf = {
        .pin_bit_mask = (1ULL << GPIO_NUM_9),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE // Falling edge
    };
    gpio_config(&io2_conf);

    // Install the ISR service
    gpio_install_isr_service(0);

    // Attach the interrupt handler
    gpio_isr_handler_add(GPIO_NUM_9, max3421_isr, (void *)GPIO_NUM_9);

    _rhport = rhport;
    return tuh_init(rhport);
}

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use.
// tuh_hid_parse_report_descriptor() can be used to parse common/simple enough
// descriptor. Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE,
// it will be skipped therefore report_desc = NULL, desc_len = 0
TU_ATTR_WEAK void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance,
                                   uint8_t const *desc_report,
                                   uint16_t desc_len)
{
    (void)dev_addr;
    (void)instance;
    (void)desc_report;
    (void)desc_len;
}

// Invoked when device with hid interface is un-mounted
TU_ATTR_WEAK void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    (void)dev_addr;
    (void)instance;
}

// Invoked when received report from device via interrupt endpoint
TU_ATTR_WEAK void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance,
                                             uint8_t const *report,
                                             uint16_t len)
{
    (void)dev_addr;
    (void)instance;
    (void)report;
    (void)len;
}

//--------------------------------------------------------------------+
// USB Host using MAX3421E
//--------------------------------------------------------------------+

static void max3421_intr_task(void *param)
{
    (void)param;

    while (1)
    {
        xSemaphoreTake(max3421_intr_sem, portMAX_DELAY);
        tuh_int_handler(1);
    }
}

static void IRAM_ATTR max3421_isr(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(max3421_intr_sem, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }
}

extern "C"
{

    void tuh_max3421_spi_cs_api(uint8_t rhport, bool active)
    {
        (void)rhport;

        if (!Adafruit_USBH_Host::_instance)
        {
            return;
        }

        if (active)
        {
            gpio_set_level(GPIO_NUM_10, 0);
        }
        else
        {
            gpio_set_level(GPIO_NUM_10, 1);
        }
    }

    bool tuh_max3421_spi_xfer_api(uint8_t rhport, uint8_t const *tx_buf,
                                  uint8_t *rx_buf, size_t xfer_bytes)
    {
        (void)rhport;

        if (!Adafruit_USBH_Host::_instance)
        {
            return false;
        }
        Adafruit_USBH_Host *host = Adafruit_USBH_Host::_instance;

        spi_transaction_t t;
        memset(&t, 0, sizeof(t));
        t.length = xfer_bytes * 8;
        t.tx_buffer = tx_buf;
        t.rx_buffer = rx_buf;

        esp_err_t ret = spi_device_polling_transmit(host->_spi, &t);
        if (ret != ESP_OK)
        {
            ESP_LOGI("USBH", "SPI transfer failed ret: %d", ret);
            return false;
        }

        return true;
    }

    void tuh_max3421_int_api(uint8_t rhport, bool enabled)
    {
        (void)rhport;

        if (!Adafruit_USBH_Host::_instance)
        {
            return;
        }
        Adafruit_USBH_Host *host = Adafruit_USBH_Host::_instance;
        (void)host;

        if (enabled)
        {
            gpio_intr_enable(GPIO_NUM_9);
        }
        else
        {
            gpio_intr_disable(GPIO_NUM_9);
        }
    }

} // extern C