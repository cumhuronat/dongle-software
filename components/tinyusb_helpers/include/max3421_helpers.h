#include "esp_rom_gpio.h"
#include "esp_mac.h"
#include "hal/gpio_ll.h"
#include "soc/usb_periph.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_private/periph_ctrl.h"
#include "driver/spi_master.h"
#include "tusb.h"
#include "macros.h"
#include "esp_log.h"

#define MAX3421_INTR_PIN GPIO_NUM_9
#define MAX3421_CS_PIN GPIO_NUM_10
#define MAX3421_SPI_HOST SPI3_HOST
#define MAX3421_MOSI_PIN GPIO_NUM_35
#define MAX3421_MISO_PIN GPIO_NUM_37
#define MAX3421_SCLK_PIN GPIO_NUM_36
#define BOARD_TUH_RHPORT 1

EXTERN_C_BEGIN
    void max3421_init(void);
    bool tuh_max3421_spi_xfer_api(uint8_t rhport, uint8_t const *tx_buf, uint8_t *rx_buf, size_t xfer_bytes);
    void tuh_max3421_spi_cs_api(uint8_t rhport, bool active);
    bool tuh_max3421_spi_xfer_api(uint8_t rhport, uint8_t const *tx_buf, uint8_t *rx_buf, size_t xfer_bytes);
    void tuh_max3421_int_api(uint8_t rhport, bool enabled);
    uint8_t tuh_max3421_reg_read(uint8_t rhport, uint8_t reg, bool in_isr);
    bool tuh_max3421_reg_write(uint8_t rhport, uint8_t reg, uint8_t data, bool in_isr);
EXTERN_C_END