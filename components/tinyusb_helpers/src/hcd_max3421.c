#include "hcd_max3421.h"

static max3421_data_t _hcd_data;

static tuh_configure_max3421_t _tuh_cfg = {
    .max_nak = MAX_NAK_DEFAULT,
    .cpuctl = 0,
    .pinctl = 0,
};

static spi_device_handle_t max3421_spi;
SemaphoreHandle_t max3421_intr_sem;
static bool intEnabled = false;

static void max3421_intr_task(void *param)
{
    (void)param;

    while (1)
    {
        if (intEnabled)
            tuh_int_handler(BOARD_TUH_RHPORT, false);
        vTaskDelay(1);
    }
}

static void spi_init(void)
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
        .max_transfer_sz = 1024};
    ESP_ERROR_CHECK(spi_bus_initialize(MAX3421_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t max3421_cfg = {
        .mode = 0,
        .clock_speed_hz = SPI_MASTER_FREQ_20M,
        .spics_io_num = MAX3421_CS_PIN,
        .queue_size = 1};
    ESP_ERROR_CHECK(spi_bus_add_device(MAX3421_SPI_HOST, &max3421_cfg, &max3421_spi));

    xTaskCreatePinnedToCore(max3421_intr_task, "max3421 intr", 2048, NULL, configMAX_PRIORITIES - 2, NULL, 1);
}

static uint8_t IRAM_ATTR reg_read(uint8_t rhport, uint8_t reg, bool in_isr)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 2 * 8;
    t.tx_data[0] = reg;
    t.tx_data[1] = 0;
    t.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
    t.rxlength = 2 * 8;

    spi_device_acquire_bus(max3421_spi, portMAX_DELAY);
    ESP_ERROR_CHECK(spi_device_polling_transmit(max3421_spi, &t));
    spi_device_release_bus(max3421_spi);
    _hcd_data.hirq = t.rx_data[0];

    return t.rx_data[1];
}

static bool IRAM_ATTR reg_write(uint8_t rhport, uint8_t reg, uint8_t data, bool in_isr)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 2 * 8;
    t.tx_data[0] = reg | CMDBYTE_WRITE;
    t.tx_data[1] = data;
    t.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;

    spi_device_acquire_bus(max3421_spi, portMAX_DELAY);
    ESP_ERROR_CHECK(spi_device_polling_transmit(max3421_spi, &t));
    spi_device_release_bus(max3421_spi);
    _hcd_data.hirq = t.rx_data[0];

    return true;
}

static void IRAM_ATTR fifo_write(uint8_t rhport, uint8_t reg, uint8_t const *buffer, uint16_t len, bool in_isr)
{
    uint8_t tx_buf[1 + len];
    tx_buf[0] = reg | CMDBYTE_WRITE;
    memcpy(&tx_buf[1], buffer, len);

    uint8_t rx_buf[1 + len];

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8 + len * 8;
    t.tx_buffer = tx_buf;
    t.rx_buffer = rx_buf;

    spi_device_acquire_bus(max3421_spi, portMAX_DELAY);
    ESP_ERROR_CHECK(spi_device_polling_transmit(max3421_spi, &t));
    spi_device_release_bus(max3421_spi);

    _hcd_data.hirq = rx_buf[0];
}

static void IRAM_ATTR fifo_read(uint8_t rhport, uint8_t *buffer, uint16_t len, bool in_isr)
{
    uint8_t const reg = RCVVFIFO_ADDR;
    uint8_t rx_buf[1 + len];

    uint8_t tx_buf[1 + len];
    tx_buf[0] = reg;

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8 + len * 8;
    t.tx_buffer = tx_buf;
    t.rx_buffer = rx_buf;

    spi_device_acquire_bus(max3421_spi, portMAX_DELAY);
    ESP_ERROR_CHECK(spi_device_polling_transmit(max3421_spi, &t));
    spi_device_release_bus(max3421_spi);
    _hcd_data.hirq = rx_buf[0];
    memcpy(buffer, rx_buf + 1, len);
}

//------------- register write helper -------------//
TU_ATTR_ALWAYS_INLINE static inline void hirq_write(uint8_t rhport, uint8_t data, bool in_isr)
{
    reg_write(rhport, HIRQ_ADDR, data, in_isr);
    // HIRQ write 1 is clear
    _hcd_data.hirq &= (uint8_t)~data;
}

TU_ATTR_ALWAYS_INLINE static inline void hien_write(uint8_t rhport, uint8_t data, bool in_isr)
{
    _hcd_data.hien = data;
    reg_write(rhport, HIEN_ADDR, data, in_isr);
}

TU_ATTR_ALWAYS_INLINE static inline void mode_write(uint8_t rhport, uint8_t data, bool in_isr)
{
    _hcd_data.mode = data;
    reg_write(rhport, MODE_ADDR, data, in_isr);
}

TU_ATTR_ALWAYS_INLINE static inline void peraddr_write(uint8_t rhport, uint8_t data, bool in_isr)
{
    if (_hcd_data.peraddr == data)
        return; // no need to change address

    _hcd_data.peraddr = data;
    reg_write(rhport, PERADDR_ADDR, data, in_isr);
}

TU_ATTR_ALWAYS_INLINE static inline void hxfr_write(uint8_t rhport, uint8_t data, bool in_isr)
{
    _hcd_data.hxfr = data;
    reg_write(rhport, HXFR_ADDR, data, in_isr);
}

TU_ATTR_ALWAYS_INLINE static inline void sndbc_write(uint8_t rhport, uint8_t data, bool in_isr)
{
    _hcd_data.sndbc = data;
    reg_write(rhport, SNDBC_ADDR, data, in_isr);
}

//--------------------------------------------------------------------+
// Endpoint helper
//--------------------------------------------------------------------+

static max3421_ep_t *find_ep_not_addr0(uint8_t daddr, uint8_t ep_num, uint8_t ep_dir)
{
    uint8_t const is_out = 1 - ep_dir;
    for (size_t i = 1; i < CFG_TUH_MAX3421_ENDPOINT_TOTAL; i++)
    {
        max3421_ep_t *ep = &_hcd_data.ep[i];
        // control endpoint is bi-direction (skip check)
        if (daddr == ep->daddr && ep_num == ep->hxfr_bm.ep_num && (ep_num == 0 || is_out == ep->hxfr_bm.is_out))
        {
            return ep;
        }
    }

    return NULL;
}

// daddr = 0 and ep_num = 0 means find a free (allocate) endpoint
TU_ATTR_ALWAYS_INLINE static inline max3421_ep_t *allocate_ep(void)
{
    return find_ep_not_addr0(0, 0, 0);
}

TU_ATTR_ALWAYS_INLINE static inline max3421_ep_t *find_opened_ep(uint8_t daddr, uint8_t ep_num, uint8_t ep_dir)
{
    if (daddr == 0 && ep_num == 0)
    {
        return &_hcd_data.ep[0];
    }
    else
    {
        return find_ep_not_addr0(daddr, ep_num, ep_dir);
    }
}

// free all endpoints belong to device address
static void free_ep(uint8_t daddr)
{
    for (size_t i = 1; i < CFG_TUH_MAX3421_ENDPOINT_TOTAL; i++)
    {
        max3421_ep_t *ep = &_hcd_data.ep[i];
        if (ep->daddr == daddr)
        {
            tu_memclr(ep, sizeof(max3421_ep_t));
        }
    }
}

// Check if endpoint has an queued transfer and not reach max NAK
TU_ATTR_ALWAYS_INLINE static inline bool is_ep_pending(max3421_ep_t const *ep)
{
    uint8_t const state = ep->state;
    return ep->packet_size && (state >= EP_STATE_ATTEMPT_1) &&
           (_tuh_cfg.max_nak == 0 || state < EP_STATE_ATTEMPT_1 + _tuh_cfg.max_nak);
}

static max3421_ep_t *find_next_pending_ep(max3421_ep_t *cur_ep)
{
    size_t const idx = (size_t)(cur_ep - _hcd_data.ep);

    for (size_t i = idx + 1; i < CFG_TUH_MAX3421_ENDPOINT_TOTAL; i++)
    {
        max3421_ep_t *ep = &_hcd_data.ep[i];
        if (is_ep_pending(ep))
        {
            return ep;
        }
    }

    for (size_t i = 0; i <= idx; i++)
    {
        max3421_ep_t *ep = &_hcd_data.ep[i];
        if (is_ep_pending(ep))
        {
            return ep;
        }
    }

    return NULL;
}

#define ACTION_LOG_SIZE 40
static char action_log[ACTION_LOG_SIZE][32]; // Store actions in fixed-length strings
static uint8_t action_log_index = 0;
static bool locked = false;

void log_action(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(action_log[action_log_index], sizeof(action_log[0]), format, args); // Formatted logging
    va_end(args);
    action_log_index = (action_log_index + 1) % ACTION_LOG_SIZE;
}

void print_action_log()
{
    ESP_LOGI("ACTION_LOG", "Recent Actions:");
    for (int i = 0; i < ACTION_LOG_SIZE; i++)
    {
        uint8_t index = (action_log_index + i) % ACTION_LOG_SIZE;
        if (strlen(action_log[index]) > 0)
        { // Check if the log entry is not empty
            ESP_LOGI("ACTION_LOG", "%s", action_log[index]);
        }
    }
}

void print_endpoint_status(void)
{
    // Buffer to hold task list information
    char taskListBuffer[1024];

    // Get task list
    vTaskList(taskListBuffer);

    // Print the task list
    ESP_LOGI("Task List", "\nTask Name\t\tState\tPrio\tStack\tTask#\n%s", taskListBuffer);
    bool isBusy = !atomic_flag_test_and_set(&_hcd_data.busy);
    if (!isBusy)
        atomic_flag_clear(&_hcd_data.busy);
    // Log controller status
    ESP_LOGI("Controller Status", "frame_count=%u, sndbc=%u, hirq=%u, hien=%u, mode=%u, peraddr=%u, hxfr=%u, busy=%d, locked=%d",
             _hcd_data.frame_count,
             _hcd_data.sndbc,
             _hcd_data.hirq,
             _hcd_data.hien,
             _hcd_data.mode,
             _hcd_data.peraddr,
             _hcd_data.hxfr,
             isBusy,
             locked);

    // Log configuration settings
    ESP_LOGI("Configuration", "max_nak=%u, cpuctl=%u, pinctl=%u",
             _tuh_cfg.max_nak,
             _tuh_cfg.cpuctl,
             _tuh_cfg.pinctl);

    // Log endpoint status
    for (size_t i = 0; i < CFG_TUH_MAX3421_ENDPOINT_TOTAL; i++)
    {
        max3421_ep_t *ep = &_hcd_data.ep[i];
        ESP_LOGI("Endpoint Status", "EP[%u]: daddr=%u, ep_num=%u, is_setup=%u, is_out=%u, is_iso=%u, state=%u, packet_size=%u, total_len=%u, xferred_len=%u, retry_count=%u, data_toggle=%u, buf=%p",
                 i,
                 ep->daddr,
                 ep->hxfr_bm.ep_num,
                 ep->hxfr_bm.is_setup,
                 ep->hxfr_bm.is_out,
                 ep->hxfr_bm.is_iso,
                 ep->state,
                 ep->packet_size,
                 ep->total_len,
                 ep->xferred_len,
                 ep->retry_count,
                 ep->data_toggle,
                 ep->buf);
    }
}

void hex_to_binary_str(uint8_t hex, char *binary_str)
{
    for (int i = 0; i < 8; i++)
    {
        binary_str[7 - i] = (hex & (1 << i)) ? '1' : '0';
    }
    binary_str[8] = '\0'; // Null-terminate the string
}

void log_general_info_as_binary(void)
{
    uint8_t hien = reg_read(1, HIEN_ADDR, false);
    uint8_t hirq = reg_read(1, HIRQ_ADDR, false);
    uint8_t mode = reg_read(1, MODE_ADDR, false);
    uint8_t peraddr = reg_read(1, PERADDR_ADDR, false);
    uint8_t hctl = reg_read(1, HCTL_ADDR, false);
    uint8_t hxfr = reg_read(1, HXFR_ADDR, false);
    uint8_t hrsl = reg_read(1, HRSL_ADDR, false);

    char binary_str[9];

    hex_to_binary_str(hien, binary_str);
    ESP_LOGI("General Info", "HIEN: 0x%02X (%s)", hien, binary_str);

    hex_to_binary_str(hirq, binary_str);
    ESP_LOGI("General Info", "HIRQ: 0x%02X (%s)", hirq, binary_str);

    hex_to_binary_str(mode, binary_str);
    ESP_LOGI("General Info", "Mode: 0x%02X (%s)", mode, binary_str);

    hex_to_binary_str(peraddr, binary_str);
    ESP_LOGI("General Info", "PERADDR: 0x%02X (%s)", peraddr, binary_str);

    hex_to_binary_str(hctl, binary_str);
    ESP_LOGI("General Info", "HCTL: 0x%02X (%s)", hctl, binary_str);

    hex_to_binary_str(hxfr, binary_str);
    ESP_LOGI("General Info", "HXFR: 0x%02X (%s)", hxfr, binary_str);

    hex_to_binary_str(hrsl, binary_str);
    ESP_LOGI("General Info", "HRSL: 0x%02X (%s)", hrsl, binary_str);
}

void log_debug_data(void)
{
    print_action_log();
    print_endpoint_status();
    log_general_info_as_binary();
}

//--------------------------------------------------------------------+
// Controller API
//--------------------------------------------------------------------+

// optional hcd configuration, called by tuh_configure()
bool hcd_configure(uint8_t rhport, uint32_t cfg_id, const void *cfg_param)
{
    (void)rhport;
    TU_VERIFY(cfg_id == TUH_CFGID_MAX3421 && cfg_param != NULL);

    tuh_configure_param_t const *cfg = (tuh_configure_param_t const *)cfg_param;
    _tuh_cfg = cfg->max3421;
    _tuh_cfg.max_nak = tu_min8(_tuh_cfg.max_nak, EP_STATE_ATTEMPT_MAX - EP_STATE_ATTEMPT_1);
    return true;
}

// Initialize controller to host mode
bool hcd_init(uint8_t rhport)
{
    (void)rhport;

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;               // Disable interrupt
    io_conf.mode = GPIO_MODE_OUTPUT;                     // Set as output mode
    io_conf.pin_bit_mask = (1ULL << MAX3421_VBUSEN_PIN); // Bit mask of the pin
    io_conf.pull_down_en = 0;                            // Disable pull-down mode
    io_conf.pull_up_en = 0;                              // Disable pull-up mode

    esp_err_t ret = gpio_config(&io_conf);

    gpio_set_level(MAX3421_VBUSEN_PIN, 1);

    hcd_int_disable(rhport);
    spi_init();

    TU_LOG2_INT(sizeof(max3421_ep_t));
    TU_LOG2_INT(sizeof(max3421_data_t));
    TU_LOG2_INT(offsetof(max3421_data_t, ep));

    tu_memclr(&_hcd_data, sizeof(_hcd_data));
    _hcd_data.peraddr = 0xff; // invalid
    _hcd_data.spi_mutex = osal_mutex_create(&_hcd_data.spi_mutexdef);
    reg_write(rhport, PINCTL_ADDR, _tuh_cfg.pinctl | PINCTL_FDUPSPI, false);

    // reset
    reg_write(rhport, USBCTL_ADDR, USBCTL_CHIPRES, false);
    reg_write(rhport, USBCTL_ADDR, 0, false);
    while (!(reg_read(rhport, USBIRQ_ADDR, false) & USBIRQ_OSCOK_IRQ))
    {
        vTaskDelay(1);
    }

    mode_write(rhport, MODE_DPPULLDN | MODE_DMPULLDN | MODE_HOST, false);
    reg_write(rhport, HCTL_ADDR, HCTL_BUSRST | HCTL_FRMRST, false);
    hirq_write(rhport, 0xff, false);
    hien_write(rhport, DEFAULT_HIEN, false);
    hcd_int_enable(rhport);

    return true;
}

bool hcd_deinit(uint8_t rhport)
{
    (void)rhport;

    // disable interrupt
    hcd_int_disable(rhport);

    // reset max3421 and power down
    reg_write(rhport, USBCTL_ADDR, USBCTL_CHIPRES, false);
    reg_write(rhport, USBCTL_ADDR, USBCTL_PWRDOWN, false);

#if OSAL_MUTEX_REQUIRED
    osal_mutex_delete(_hcd_data.spi_mutex);
    _hcd_data.spi_mutex = NULL;
#endif

    return true;
}

void hcd_int_enable(uint8_t rhport)
{
    intEnabled = true;
}

void hcd_int_disable(uint8_t rhport)
{
    intEnabled = false;
}

// Get frame number (1ms)
uint32_t hcd_frame_number(uint8_t rhport)
{
    (void)rhport;
    return (uint32_t)_hcd_data.frame_count;
}

//--------------------------------------------------------------------+
// Port API
//--------------------------------------------------------------------+

// Get the current connect status of roothub port
bool hcd_port_connect_status(uint8_t rhport)
{
    (void)rhport;
    return (_hcd_data.mode & MODE_SOFKAENAB) ? true : false;
}

// Reset USB bus on the port. Return immediately, bus reset sequence may not be complete.
// Some port would require hcd_port_reset_end() to be invoked after 10ms to complete the reset sequence.
void hcd_port_reset(uint8_t rhport)
{
    reg_write(rhport, HCTL_ADDR, HCTL_BUSRST, false);
}

// Complete bus reset sequence, may be required by some controllers
void hcd_port_reset_end(uint8_t rhport)
{
    reg_write(rhport, HCTL_ADDR, 0, false);
}

// Get port link speed
tusb_speed_t hcd_port_speed_get(uint8_t rhport)
{
    (void)rhport;
    return (_hcd_data.mode & MODE_LOWSPEED) ? TUSB_SPEED_LOW : TUSB_SPEED_FULL;
}

// HCD closes all opened endpoints belong to this device
void hcd_device_close(uint8_t rhport, uint8_t dev_addr)
{
    (void)rhport;
    (void)dev_addr;
}

//--------------------------------------------------------------------+
// Endpoints API
//--------------------------------------------------------------------+

// Open an endpoint
bool hcd_edpt_open(uint8_t rhport, uint8_t daddr, tusb_desc_endpoint_t const *ep_desc)
{
    (void)rhport;

    uint8_t const ep_num = tu_edpt_number(ep_desc->bEndpointAddress);
    tusb_dir_t const ep_dir = tu_edpt_dir(ep_desc->bEndpointAddress);
    max3421_ep_t *ep;
    if (daddr == 0 && ep_num == 0)
    {
        ep = &_hcd_data.ep[0];
    }
    else
    {
        ep = allocate_ep();
        TU_ASSERT(ep);
        ep->daddr = daddr;
        ep->hxfr_bm.ep_num = (uint8_t)(ep_num & 0x0f);
        ep->hxfr_bm.is_out = (ep_dir == TUSB_DIR_OUT) ? 1 : 0;
        ep->hxfr_bm.is_iso = (TUSB_XFER_ISOCHRONOUS == ep_desc->bmAttributes.xfer) ? 1 : 0;
    }

    ep->packet_size = (uint16_t)(tu_edpt_packet_size(ep_desc) & 0x7ff);

    return true;
}

static void xact_out(uint8_t rhport, max3421_ep_t *ep, bool switch_ep, bool in_isr)
{
    uint8_t const xact_len = (uint8_t)tu_min16(ep->total_len - ep->xferred_len, ep->packet_size);
    if (switch_ep)
    {
        peraddr_write(rhport, ep->daddr, in_isr);
        uint8_t const hctl = (ep->data_toggle ? HCTL_SNDTOG1 : HCTL_SNDTOG0);
        reg_write(rhport, HCTL_ADDR, hctl, in_isr);
    }

    if (!ep->retry_count)
    {
        if (xact_len > 0)
        {
            if (!(_hcd_data.hirq & HIRQ_SNDBAV_IRQ))
            {
                ESP_LOGE("MAX3421", "no SNDBAV_IRQ");
            }
            TU_ASSERT(_hcd_data.hirq & HIRQ_SNDBAV_IRQ, );
            fifo_write(rhport, SNDFIFO_ADDR, ep->buf, xact_len, in_isr);
        }
        sndbc_write(rhport, xact_len, in_isr);
    }

    hxfr_write(rhport, ep->hxfr, in_isr);
}

static void xact_in(uint8_t rhport, max3421_ep_t *ep, bool switch_ep, bool in_isr)
{
    if (switch_ep)
    {
        peraddr_write(rhport, ep->daddr, in_isr);

        uint8_t const hctl = (ep->data_toggle ? HCTL_RCVTOG1 : HCTL_RCVTOG0);
        reg_write(rhport, HCTL_ADDR, hctl, in_isr);
    }
    hxfr_write(rhport, ep->hxfr, in_isr);
}

static void xact_setup(uint8_t rhport, max3421_ep_t *ep, bool in_isr)
{
    peraddr_write(rhport, ep->daddr, in_isr);
    fifo_write(rhport, SUDFIFO_ADDR, ep->buf, 8, in_isr);
    hxfr_write(rhport, HXFR_SETUP, in_isr);
}

static void xact_generic(uint8_t rhport, max3421_ep_t *ep, bool switch_ep, bool in_isr)
{
    if (ep->hxfr_bm.ep_num == 0)
    {
        // setup
        if (ep->hxfr_bm.is_setup)
        {
            xact_setup(rhport, ep, in_isr);
            return;
        }

        // status
        if (ep->buf == NULL || ep->total_len == 0)
        {
            uint8_t const hxfr = (uint8_t)(HXFR_HS | (ep->hxfr & HXFR_OUT_NIN));
            peraddr_write(rhport, ep->daddr, in_isr);
            hxfr_write(rhport, hxfr, in_isr);
            return;
        }
    }

    if (ep->hxfr_bm.is_out)
    {
        xact_out(rhport, ep, switch_ep, in_isr);
    }
    else
    {
        xact_in(rhport, ep, switch_ep, in_isr);
    }
}

// Submit a transfer, when complete hcd_event_xfer_complete() must be invoked
bool hcd_edpt_xfer(uint8_t rhport, uint8_t daddr, uint8_t ep_addr, uint8_t *buffer, uint16_t buflen)
{

    uint8_t const ep_num = tu_edpt_number(ep_addr);
    uint8_t const ep_dir = (uint8_t)tu_edpt_dir(ep_addr);

    max3421_ep_t *ep = find_opened_ep(daddr, ep_num, ep_dir);
    TU_VERIFY(ep);

    if (ep_num == 0)
    {
        // control transfer can switch direction
        ep->hxfr_bm.is_out = ep_dir ? 0 : 1;
        ep->hxfr_bm.is_setup = 0;
        ep->data_toggle = 1;
    }

    ep->buf = buffer;
    ep->total_len = buflen;
    ep->xferred_len = 0;
    ep->state = EP_STATE_ATTEMPT_1;
    ep->retry_count = 0;

    // carry out transfer if not busy
    if (!atomic_flag_test_and_set(&_hcd_data.busy))
    {
        xact_generic(rhport, ep, true, false);
    }

    return true;
}

bool hcd_edpt_abort_xfer(uint8_t rhport, uint8_t daddr, uint8_t ep_addr)
{
    uint8_t const ep_num = tu_edpt_number(ep_addr);
    uint8_t const ep_dir = (uint8_t)tu_edpt_dir(ep_addr);
    max3421_ep_t *ep = find_opened_ep(daddr, ep_num, ep_dir);
    TU_VERIFY(ep);
    debugLog("abort %d", ep->hxfr_bm.ep_num);

    if (EP_STATE_ATTEMPT_1 <= ep->state && ep->state < EP_STATE_ATTEMPT_MAX)
    {
        hcd_int_disable(rhport);
        ep->state = EP_STATE_ABORTING;
        hcd_int_enable(rhport);
    }

    return true;
}

// Submit a special transfer to send 8-byte Setup Packet, when complete hcd_event_xfer_complete() must be invoked
bool hcd_setup_send(uint8_t rhport, uint8_t daddr, uint8_t const setup_packet[8])
{
    (void)rhport;

    max3421_ep_t *ep = find_opened_ep(daddr, 0, 0);
    TU_ASSERT(ep);

    ep->hxfr_bm.is_out = 1;
    ep->hxfr_bm.is_setup = 1;
    ep->buf = (uint8_t *)(uintptr_t)setup_packet;
    ep->total_len = 8;
    ep->xferred_len = 0;
    ep->state = EP_STATE_ATTEMPT_1;
    // carry out transfer if not busy
    if (!atomic_flag_test_and_set(&_hcd_data.busy))
    {
        xact_setup(rhport, ep, false);
    }

    return true;
}

// clear stall, data toggle is also reset to DATA0
bool hcd_edpt_clear_stall(uint8_t rhport, uint8_t dev_addr, uint8_t ep_addr)
{
    (void)rhport;
    (void)dev_addr;
    (void)ep_addr;

    return false;
}

//--------------------------------------------------------------------+
// Interrupt Handler
//--------------------------------------------------------------------+

static void handle_connect_irq(uint8_t rhport, bool in_isr)
{
    uint8_t const hrsl = reg_read(rhport, HRSL_ADDR, in_isr);
    uint8_t const jk = hrsl & (HRSL_JSTATUS | HRSL_KSTATUS);

    uint8_t new_mode = MODE_DPPULLDN | MODE_DMPULLDN | MODE_HOST;
    TU_LOG2_HEX(jk);

    switch (jk)
    {
    case 0x00:                          // SEO is disconnected
    case (HRSL_JSTATUS | HRSL_KSTATUS): // SE1 is illegal
        mode_write(rhport, new_mode, in_isr);

        // port reset anyway, this will help to stable bus signal for next connection
        reg_write(rhport, HCTL_ADDR, HCTL_BUSRST, in_isr);
        hcd_event_device_remove(rhport, in_isr);
        ESP_LOGI("HCDMAX", "HUB Port %u Detach", rhport);
        reg_write(rhport, HCTL_ADDR, 0, in_isr);
        break;

    default:
    {
        // Bus Reset also cause CONDET IRQ, skip if we are already connected and doing bus reset
        if ((_hcd_data.hirq & HIRQ_BUSEVENT_IRQ) && (_hcd_data.mode & MODE_SOFKAENAB))
        {
            break;
        }

        // Low speed if (LS = 1 and J-state) or (LS = 0 and K-State)
        // However, since we are always in full speed mode, we can just check J-state
        if (jk == HRSL_KSTATUS)
        {
            new_mode |= MODE_LOWSPEED;
            TU_LOG3("Low speed\r\n");
        }
        else
        {
            TU_LOG3("Full speed\r\n");
        }
        new_mode |= MODE_SOFKAENAB;
        mode_write(rhport, new_mode, in_isr);

        // FIXME multiple MAX3421 rootdevice address is not 1
        uint8_t const daddr = 1;
        free_ep(daddr);

        ESP_LOGI("HCDMAX", "HUB Port %u Attach", rhport);

        hcd_event_device_attach(rhport, in_isr);
        break;
    }
    }
}

static void xfer_complete_isr(uint8_t rhport, max3421_ep_t *ep, xfer_result_t result, uint8_t hrsl, bool in_isr)
{
    uint8_t const ep_dir = 1 - ep->hxfr_bm.is_out;
    uint8_t const ep_addr = tu_edpt_addr(ep->hxfr_bm.ep_num, ep_dir);

    // save data toggle
    if (ep_dir)
    {
        ep->data_toggle = (hrsl & HRSL_RCVTOGRD) ? 1u : 0u;
    }
    else
    {
        ep->data_toggle = (hrsl & HRSL_SNDTOGRD) ? 1u : 0u;
    }
    ep->state = EP_STATE_IDLE;
    hcd_event_xfer_complete(ep->daddr, ep_addr, ep->xferred_len, result, in_isr);

    // Find next pending endpoint
    max3421_ep_t *next_ep = find_next_pending_ep(ep);
    if (next_ep)
    {
        xact_generic(rhport, next_ep, true, in_isr);
    }
    else
    {
        // no more pending
        atomic_flag_clear(&_hcd_data.busy);
    }
}

static void handle_xfer_done(uint8_t rhport, bool in_isr)
{
    uint8_t const hrsl = reg_read(rhport, HRSL_ADDR, in_isr);
    uint8_t const hresult = hrsl & HRSL_RESULT_MASK;
    uint8_t const ep_num = _hcd_data.hxfr & HXFR_EPNUM_MASK;
    uint8_t const hxfr_type = _hcd_data.hxfr & 0xf0;
    uint8_t const ep_dir = ((hxfr_type & HXFR_SETUP) || (hxfr_type & HXFR_OUT_NIN)) ? 0 : 1;

    max3421_ep_t *ep = find_opened_ep(_hcd_data.peraddr, ep_num, ep_dir);

    TU_VERIFY(ep, );

    if (ep_dir)
    {
        ep->data_toggle = (hrsl & HRSL_RCVTOGRD) ? 1u : 0u;
    }
    else
    {
        ep->data_toggle = (hrsl & HRSL_SNDTOGRD) ? 1u : 0u;
    }

    xfer_result_t xfer_result;
    switch (hresult)
    {
    case HRSL_NAK:
        ep->retry_count++;
        if (ep->state == EP_STATE_ABORTING)
        {
            ep->state = EP_STATE_IDLE;
        }
        else
        {
            if (ep_num == 0)
            {
                // control endpoint -> retry immediately and return
                hxfr_write(rhport, _hcd_data.hxfr, in_isr);
                return;
            }
            else if (EP_STATE_ATTEMPT_1 <= ep->state && ep->state < EP_STATE_ATTEMPT_MAX)
            {
                ep->state++;
            }
        }

        max3421_ep_t *next_ep = find_next_pending_ep(ep);
        if (ep == next_ep)
        {
            hxfr_write(rhport, _hcd_data.hxfr, in_isr);
        }
        else if (next_ep)
        {
            xact_generic(rhport, next_ep, true, in_isr);
        }
        else
        {
            // no more pending in this frame -> clear busy
            atomic_flag_clear(&_hcd_data.busy);
        }
        return;

    case HRSL_BAD_REQ:
        ESP_LOGE("MAX3421", "HRSL_BAD_REQ");
        // occurred when initialized without any pending transfer. Skip for now
        return;

    case HRSL_SUCCESS:
        xfer_result = XFER_RESULT_SUCCESS;
        break;

    case HRSL_STALL:
        ESP_LOGE("MAX3421", "HRSL_STALL");
        xfer_result = XFER_RESULT_STALLED;
        break;

    default:
        ESP_LOGE("MAX3421", "HRSL: %02X", hrsl);
        TU_LOG3("HRSL: %02X\r\n", hrsl);
        xfer_result = XFER_RESULT_FAILED;
        break;
    }

    if (xfer_result != XFER_RESULT_SUCCESS)
    {
        xfer_complete_isr(rhport, ep, xfer_result, hrsl, in_isr);
        return;
    }

    if (ep_dir)
    {
        if (hxfr_type & HXFR_HS)
        {
            ep->state = EP_STATE_COMPLETE;
        }

        if (ep->state == EP_STATE_COMPLETE)
        {
            xfer_complete_isr(rhport, ep, xfer_result, hrsl, in_isr);
        }
        else
        {
            hxfr_write(rhport, _hcd_data.hxfr, in_isr);
        }
    }
    else
    {
        uint8_t xact_len;

        if (hxfr_type & HXFR_SETUP)
        {
            xact_len = 8;
        }
        else if (hxfr_type & HXFR_HS)
        {
            xact_len = 0;
        }
        else
        {
            xact_len = _hcd_data.sndbc;
        }

        ep->xferred_len += xact_len;
        ep->buf += xact_len;
        ep->retry_count = 0;

        if (xact_len < ep->packet_size || ep->xferred_len >= ep->total_len)
        {
            xfer_complete_isr(rhport, ep, xfer_result, hrsl, in_isr);
        }
        else
        {
            xact_out(rhport, ep, false, in_isr);
        }
    }
}

void hcd_int_handler(uint8_t rhport, bool in_isr)
{

    uint8_t hirq = reg_read(rhport, HIRQ_ADDR, in_isr) & _hcd_data.hien;
    if (!hirq)
    {
        return;
    }

    if (hirq & HIRQ_FRAME_IRQ)
    {
        _hcd_data.frame_count++;
        max3421_ep_t *ep_retry = NULL;
        for (size_t i = 0; i < CFG_TUH_MAX3421_ENDPOINT_TOTAL; i++)
        {
            max3421_ep_t *ep = &_hcd_data.ep[i];
            if (ep->packet_size && ep->state > EP_STATE_ATTEMPT_1)
            {
                ep->state = EP_STATE_ATTEMPT_1;

                if (ep_retry == NULL)
                {
                    ep_retry = ep;
                }
            }
        }

        if (ep_retry != NULL && !atomic_flag_test_and_set(&_hcd_data.busy))
        {
            xact_generic(rhport, ep_retry, true, in_isr);
        }
        hirq_write(rhport, HIRQ_FRAME_IRQ, in_isr);
    }

    if (hirq & HIRQ_CONDET_IRQ)
    {
        handle_connect_irq(rhport, in_isr);
        hirq_write(rhport, HIRQ_CONDET_IRQ, in_isr);
    }

    while (hirq & (HIRQ_RCVDAV_IRQ | HIRQ_HXFRDN_IRQ))
    {
        if (hirq & HIRQ_RCVDAV_IRQ)
        {

            uint8_t const ep_num = _hcd_data.hxfr & HXFR_EPNUM_MASK;
            max3421_ep_t *ep = find_opened_ep(_hcd_data.peraddr, ep_num, 1);
            uint8_t xact_len = 0;

            // RCVDAV_IRQ can trigger 2 times (dual buffered)
            while (hirq & HIRQ_RCVDAV_IRQ)
            {
                uint8_t rcvbc = reg_read(rhport, RCVBC_ADDR, in_isr);
                xact_len = (uint8_t)tu_min16(rcvbc, ep->total_len - ep->xferred_len);
                if (xact_len)
                {
                    fifo_read(rhport, ep->buf, xact_len, in_isr);
                    ep->buf += xact_len;
                    ep->xferred_len += xact_len;
                }

                // ack RCVDVAV IRQ
                hirq_write(rhport, HIRQ_RCVDAV_IRQ, in_isr);
                hirq = reg_read(rhport, HIRQ_ADDR, in_isr);
            }

            if (xact_len < ep->packet_size || ep->xferred_len >= ep->total_len)
            {
                ep->state = EP_STATE_COMPLETE;
            }
        }

        if (hirq & HIRQ_HXFRDN_IRQ)
        {
            hirq_write(rhport, HIRQ_HXFRDN_IRQ, in_isr);
            handle_xfer_done(rhport, in_isr);
        }

        hirq = reg_read(rhport, HIRQ_ADDR, in_isr);
    }
}
