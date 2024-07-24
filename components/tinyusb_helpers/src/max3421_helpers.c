#include "max3421_helpers.h"

static spi_device_handle_t max3421_spi;
SemaphoreHandle_t max3421_intr_sem;

static void IRAM_ATTR max3421_isr_handler(void *arg)
{
    (void)arg;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(max3421_intr_sem, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }
}

static void max3421_intr_task(void *param)
{
    (void)param;

    while (1)
    {
        xSemaphoreTake(max3421_intr_sem, portMAX_DELAY);
        tuh_int_handler(BOARD_TUH_RHPORT, false);
    }
}

void max3421_init(void)
{
    gpio_set_direction(MAX3421_CS_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(MAX3421_CS_PIN, 1);

    spi_bus_config_t buscfg = {
        .miso_io_num = MAX3421_MISO_PIN,
        .mosi_io_num = MAX3421_MOSI_PIN,
        .sclk_io_num = MAX3421_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .data4_io_num = -1,
        .data5_io_num = -1,
        .data6_io_num = -1,
        .data7_io_num = -1,
        .max_transfer_sz = 1024
        };
    ESP_ERROR_CHECK(spi_bus_initialize(MAX3421_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t max3421_cfg = {
        .mode = 0,
        .clock_speed_hz = SPI_MASTER_FREQ_20M,
        .spics_io_num = -1,
        .queue_size = 1};
    ESP_ERROR_CHECK(spi_bus_add_device(MAX3421_SPI_HOST, &max3421_cfg, &max3421_spi));

    max3421_intr_sem = xSemaphoreCreateBinary();
    xTaskCreate(max3421_intr_task, "max3421 intr", 2048, NULL, configMAX_PRIORITIES - 2, NULL);

    gpio_set_direction(MAX3421_INTR_PIN, GPIO_MODE_INPUT);
    gpio_set_intr_type(MAX3421_INTR_PIN, GPIO_INTR_NEGEDGE);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(MAX3421_INTR_PIN, max3421_isr_handler, NULL);
}

void tuh_max3421_int_api(uint8_t rhport, bool enabled)
{
    (void)rhport;
    if (enabled)
    {
        gpio_intr_enable(MAX3421_INTR_PIN);
    }
    else
    {
        gpio_intr_disable(MAX3421_INTR_PIN);
    }
}

void tuh_max3421_spi_cs_api(uint8_t rhport, bool active)
{
    (void)rhport;
    gpio_set_level(MAX3421_CS_PIN, active ? 0 : 1);
}

bool tuh_max3421_spi_xfer_api(uint8_t rhport, uint8_t const *tx_buf, uint8_t *rx_buf, size_t xfer_bytes)
{
    (void)rhport;

    if (tx_buf == NULL)
    {
        tx_buf = rx_buf;
    }

    size_t const len_bits = xfer_bytes << 3;

    spi_transaction_t xact = {
        .length = len_bits,
        .rxlength = rx_buf ? len_bits : 0,
        .tx_buffer = tx_buf,
        .rx_buffer = rx_buf};

    ESP_ERROR_CHECK(spi_device_polling_transmit(max3421_spi, &xact));
    return true;
}
