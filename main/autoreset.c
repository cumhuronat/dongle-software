#include "autoreset.h"

static bool usb_persist_enabled = false;

esp_err_t deinit_usb_hal(usb_phy_handle_t phy_handle)
{
    esp_err_t ret = ESP_OK;
    if (phy_handle)
    {
        ret = usb_del_phy(phy_handle);
        if (ret != ESP_OK)
        {
            ESP_LOGE("AutoRes", "Failed to deinit USB PHY");
        }
    }
    return ret;
}

static void hw_cdc_reset_handler(void *arg)
{
    portBASE_TYPE xTaskWoken = 0;
    uint32_t usbjtag_intr_status = usb_serial_jtag_ll_get_intsts_mask();
    usb_serial_jtag_ll_clr_intsts_mask(usbjtag_intr_status);

    if (usbjtag_intr_status & USB_SERIAL_JTAG_INTR_BUS_RESET)
    {
        xSemaphoreGiveFromISR((SemaphoreHandle_t)arg, &xTaskWoken);
    }

    if (xTaskWoken == pdTRUE)
    {
        portYIELD_FROM_ISR();
    }
}

static void IRAM_ATTR usb_persist_shutdown_handler(void)
{
    REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
}

void usb_persist_restart(usb_phy_handle_t phy_handle)
{
    if (esp_register_shutdown_handler(usb_persist_shutdown_handler) == ESP_OK)
    {
        usb_switch_to_cdc_jtag(phy_handle);
        esp_restart();
    }
    else{
        ESP_LOGE("AutoRes", "Failed to register shutdown handler");
    }
}

void usb_switch_to_cdc_jtag(usb_phy_handle_t phy_handle)
{
    // Disable USB-OTG
    deinit_usb_hal(phy_handle);
    periph_ll_reset(PERIPH_USB_MODULE);
    // periph_ll_enable_clk_clear_rst(PERIPH_USB_MODULE);
    periph_ll_disable_clk_set_rst(PERIPH_USB_MODULE);

    // Switch to hardware CDC+JTAG
    CLEAR_PERI_REG_MASK(RTC_CNTL_USB_CONF_REG, (RTC_CNTL_SW_HW_USB_PHY_SEL | RTC_CNTL_SW_USB_PHY_SEL | RTC_CNTL_USB_PAD_ENABLE));

    // Do not use external PHY
    CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_PHY_SEL);

    // Release GPIO pins from  CDC+JTAG
    CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_USB_PAD_ENABLE);

    gpio_set_direction(USBPHY_DM_NUM, GPIO_MODE_OUTPUT_OD);
    gpio_set_direction(USBPHY_DP_NUM, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(USBPHY_DM_NUM, 0);
    gpio_set_level(USBPHY_DP_NUM, 0);

    usb_fsls_phy_ll_int_jtag_enable(&USB_SERIAL_JTAG);

    usb_serial_jtag_ll_disable_intr_mask(USB_SERIAL_JTAG_LL_INTR_MASK);
    usb_serial_jtag_ll_clr_intsts_mask(USB_SERIAL_JTAG_LL_INTR_MASK);
    usb_serial_jtag_ll_ena_intr_mask(USB_SERIAL_JTAG_INTR_BUS_RESET);
    intr_handle_t intr_handle = NULL;
    SemaphoreHandle_t reset_sem = xSemaphoreCreateBinary();
    if (reset_sem)
    {
        if (esp_intr_alloc(ETS_USB_SERIAL_JTAG_INTR_SOURCE, 0, hw_cdc_reset_handler, reset_sem, &intr_handle) != ESP_OK)
        {
            vSemaphoreDelete(reset_sem);
            reset_sem = NULL;
            ESP_LOGE("AutoRes", "HW USB CDC failed to init interrupts");
        }
    }
    else
    {
        ESP_LOGE("AutoRes", "reset_sem init failed");
    }

    // Connect GPIOs to integrated CDC+JTAG
    SET_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_USB_PAD_ENABLE);

    // Wait for BUS_RESET to give us back the semaphore
    if (reset_sem)
    {
        if (xSemaphoreTake(reset_sem, 3000 / portTICK_PERIOD_MS) != pdPASS)
        {
            ESP_LOGE("AutoRes", "reset_sem timeout");
        }
        usb_serial_jtag_ll_disable_intr_mask(USB_SERIAL_JTAG_LL_INTR_MASK);
        esp_intr_free(intr_handle);
        vSemaphoreDelete(reset_sem);
    }
}