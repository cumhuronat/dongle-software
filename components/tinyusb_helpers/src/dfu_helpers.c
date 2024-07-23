#include "dfu_helpers.h"

#define FLASH_SECTOR_SIZE 4096
#define BUFFER_SIZE 4096

static char* TAG = "esp_dfu";

uint32_t tud_dfu_get_timeout_cb(uint8_t alt, uint8_t state)
{
    if (state == DFU_DNBUSY) {
        // For this example
        // - Atl0 Flash is fast : 1   ms
        // - Alt1 EEPROM is slow: 100 ms
        return (alt == 0) ? 1 : 100;
    } else if (state == DFU_MANIFEST) {
        // since we don't buffer entire image and do any flashing in manifest stage
        return 0;
    }

    return 0;
}

void tud_dfu_download_cb(uint8_t alt, uint16_t block_num, uint8_t const* data, uint16_t length)
{
    (void) alt;
    (void) block_num;

    ESP_LOGI(TAG, "Received Alt %u BlockNum %u of length %u", alt, block_num, length);

    esp_err_t ret = ota_start(data, length);
    if (ret == ESP_OK) {
        tud_dfu_finish_flashing(DFU_STATUS_OK);
    } else {
        tud_dfu_finish_flashing(DFU_STATUS_ERR_WRITE);
    }
}

void tud_dfu_manifest_cb(uint8_t alt)
{
    (void) alt;
    ESP_LOGI(TAG, "Download completed, enter manifestation\r\n");

    esp_err_t ret = ota_complete();
    // flashing op for manifest is complete without error
    // Application can perform checksum, should it fail, use appropriate status such as errVERIFY.

    if (ret == ESP_OK) {
        tud_dfu_finish_flashing(DFU_STATUS_OK);
    } else {
        tud_dfu_finish_flashing(DFU_STATUS_ERR_VERIFY);
    }
}