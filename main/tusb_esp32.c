#include "esp_rom_gpio.h"
#include "esp_mac.h"
#include "hal/gpio_ll.h"
#include "soc/usb_periph.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_private/periph_ctrl.h"
#include "driver/spi_master.h"
#include "tusb.h"


#define MAX3421_INTR_PIN GPIO_NUM_9
#define MAX3421_CS_PIN GPIO_NUM_10
#define MAX3421_SPI_HOST SPI3_HOST
#define MAX3421_MOSI_PIN    GPIO_NUM_35
#define MAX3421_MISO_PIN    GPIO_NUM_37
#define MAX3421_SCLK_PIN    GPIO_NUM_36
#define BOARD_TUH_RHPORT 1

static spi_device_handle_t max3421_spi;
SemaphoreHandle_t max3421_intr_sem;

static void IRAM_ATTR max3421_isr_handler(void *arg)
{
    (void)arg; // arg is gpio num

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
    // CS pin
    gpio_set_direction(MAX3421_CS_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(MAX3421_CS_PIN, 1);    

    // SPI
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
        .max_transfer_sz = 1024};
    ESP_ERROR_CHECK(spi_bus_initialize(MAX3421_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t max3421_cfg = {
        .mode = 0,
        .clock_speed_hz = 20000000, // S2/S3 can work with 26 Mhz, but esp32 seems only work up to 20 Mhz
        .spics_io_num = -1,         // manual control CS
        .queue_size = 1};
    ESP_ERROR_CHECK(spi_bus_add_device(MAX3421_SPI_HOST, &max3421_cfg, &max3421_spi));

    // Interrupt pin
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
        // fifo read, transmit rx_buf as dummy
        tx_buf = rx_buf;
    }

    // length in bits
    size_t const len_bits = xfer_bytes << 3;

    spi_transaction_t xact = {
        .length = len_bits,
        .rxlength = rx_buf ? len_bits : 0,
        .tx_buffer = tx_buf,
        .rx_buffer = rx_buf};

    ESP_ERROR_CHECK(spi_device_transmit(max3421_spi, &xact));
    return true;
}
