#include "autoreset.h"

static usb_phy_handle_t phy_hdl;
static usb_phy_config_t phy_conf;
static const char *TAG = "AUTO_RESET";


void switch_to_jtag(void)
{
    REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
    usb_del_phy(phy_hdl);
    phy_conf.controller = USB_PHY_CTRL_SERIAL_JTAG;
    phy_conf.otg_mode = USB_PHY_MODE_DEFAULT;
    usb_new_phy(&phy_conf, &phy_hdl);
    esp_restart();
}

void switch_to_tinyusb(void)
{
    ESP_LOGI(TAG, "Switching to TinyUSB mode");
    memset(&phy_conf, 0, sizeof(phy_conf));
    phy_conf.controller = USB_PHY_CTRL_OTG;
    phy_conf.target = USB_PHY_TARGET_INT;
    phy_conf.otg_mode = USB_OTG_MODE_DEVICE;

    usb_new_phy(&phy_conf, &phy_hdl);
}