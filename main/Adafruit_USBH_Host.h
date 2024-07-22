#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "driver/spi_master.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "tusb.h"
#include "driver/gpio.h"

extern "C"
{
    void tuh_max3421_spi_cs_api(uint8_t rhport, bool active);
    bool tuh_max3421_spi_xfer_api(uint8_t rhport, uint8_t const *tx_buf,
                                  uint8_t *rx_buf, size_t xfer_bytes);
    void tuh_max3421_int_api(uint8_t rhport, bool enabled);
}

class Adafruit_USBH_Host
{
private:
    spi_device_handle_t _spi;

public:
    // constructor for using MAX3421E (host shield)
    Adafruit_USBH_Host();

    uint8_t max3421_readRegister(uint8_t reg, bool in_isr);
    bool max3421_writeRegister(uint8_t reg, uint8_t data, bool in_isr);
    bool max3421_writeIOPINS1(uint8_t data, bool in_isr)
    {
        enum
        {
            IOPINS1_ADDR = 20u << 3
        }; // 0xA0
        return max3421_writeRegister(IOPINS1_ADDR, data, in_isr);
    }
    bool max3421_writeIOPINS2(uint8_t data, bool in_isr)
    {
        enum
        {
            IOPINS2_ADDR = 21u << 3
        }; // 0xA8
        return max3421_writeRegister(IOPINS2_ADDR, data, in_isr);
    }

    bool configure(uint8_t rhport, uint32_t cfg_id, const void *cfg_param);
    bool begin(uint8_t rhport);
    void task(uint32_t timeout_ms = UINT32_MAX, bool in_isr = false);

    static Adafruit_USBH_Host *_instance;

private:
    friend void tuh_max3421_spi_cs_api(uint8_t rhport, bool active);
    friend bool tuh_max3421_spi_xfer_api(uint8_t rhport, uint8_t const *tx_buf,
                                         uint8_t *rx_buf, size_t xfer_bytes);
    friend void tuh_max3421_int_api(uint8_t rhport, bool enabled);
    uint8_t _rhport;
};
