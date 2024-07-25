#pragma once

#include <stdatomic.h>
#include "host/hcd.h"
#include "host/usbh.h"
#include "esp_log.h"
#include "esp_debug_helpers.h"
#include "debug_helpers.h"
#include "max3421_helpers.h"
#include "macros.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_gpio.h"
#include "esp_mac.h"
#include "hal/gpio_ll.h"
#include "soc/usb_periph.h"
#include "driver/gpio.h"
#include "esp_private/periph_ctrl.h"
#include "driver/spi_master.h"
#include "esp_attr.h"


enum
{
    CMDBYTE_WRITE = 0x02,
};

enum
{
    RCVVFIFO_ADDR = 1u << 3,  // 0x08
    SNDFIFO_ADDR = 2u << 3,   // 0x10
    SUDFIFO_ADDR = 4u << 3,   // 0x20
    RCVBC_ADDR = 6u << 3,     // 0x30
    SNDBC_ADDR = 7u << 3,     // 0x38
    USBIRQ_ADDR = 13u << 3,   // 0x68
    USBIEN_ADDR = 14u << 3,   // 0x70
    USBCTL_ADDR = 15u << 3,   // 0x78
    CPUCTL_ADDR = 16u << 3,   // 0x80
    PINCTL_ADDR = 17u << 3,   // 0x88
    REVISION_ADDR = 18u << 3, // 0x90
    // 19 is not used
    IOPINS1_ADDR = 20u << 3, // 0xA0
    IOPINS2_ADDR = 21u << 3, // 0xA8
    GPINIRQ_ADDR = 22u << 3, // 0xB0
    GPINIEN_ADDR = 23u << 3, // 0xB8
    GPINPOL_ADDR = 24u << 3, // 0xC0
    HIRQ_ADDR = 25u << 3,    // 0xC8
    HIEN_ADDR = 26u << 3,    // 0xD0
    MODE_ADDR = 27u << 3,    // 0xD8
    PERADDR_ADDR = 28u << 3, // 0xE0
    HCTL_ADDR = 29u << 3,    // 0xE8
    HXFR_ADDR = 30u << 3,    // 0xF0
    HRSL_ADDR = 31u << 3,    // 0xF8
};

enum
{
    USBIRQ_OSCOK_IRQ = 1u << 0,
    USBIRQ_NOVBUS_IRQ = 1u << 5,
    USBIRQ_VBUS_IRQ = 1u << 6,
};

enum
{
    USBCTL_PWRDOWN = 1u << 4,
    USBCTL_CHIPRES = 1u << 5,
};

enum
{
    CPUCTL_IE = 1u << 0,
    CPUCTL_PULSEWID0 = 1u << 6,
    CPUCTL_PULSEWID1 = 1u << 7,
};

enum
{
    PINCTL_GPXA = 1u << 0,
    PINCTL_GPXB = 1u << 1,
    PINCTL_POSINT = 1u << 2,
    PINCTL_INTLEVEL = 1u << 3,
    PINCTL_FDUPSPI = 1u << 4,
};

enum
{
    HIRQ_BUSEVENT_IRQ = 1u << 0,
    HIRQ_RWU_IRQ = 1u << 1,
    HIRQ_RCVDAV_IRQ = 1u << 2,
    HIRQ_SNDBAV_IRQ = 1u << 3,
    HIRQ_SUSDN_IRQ = 1u << 4,
    HIRQ_CONDET_IRQ = 1u << 5,
    HIRQ_FRAME_IRQ = 1u << 6,
    HIRQ_HXFRDN_IRQ = 1u << 7,
};

enum
{
    MODE_HOST = 1u << 0,
    MODE_LOWSPEED = 1u << 1,
    MODE_HUBPRE = 1u << 2,
    MODE_SOFKAENAB = 1u << 3,
    MODE_SEPIRQ = 1u << 4,
    MODE_DELAYISO = 1u << 5,
    MODE_DMPULLDN = 1u << 6,
    MODE_DPPULLDN = 1u << 7,
};

enum
{
    HCTL_BUSRST = 1u << 0,
    HCTL_FRMRST = 1u << 1,
    HCTL_SAMPLEBUS = 1u << 2,
    HCTL_SIGRSM = 1u << 3,
    HCTL_RCVTOG0 = 1u << 4,
    HCTL_RCVTOG1 = 1u << 5,
    HCTL_SNDTOG0 = 1u << 6,
    HCTL_SNDTOG1 = 1u << 7,
};

enum
{
    HXFR_EPNUM_MASK = 0x0f,
    HXFR_SETUP = 1u << 4,
    HXFR_OUT_NIN = 1u << 5,
    HXFR_ISO = 1u << 6,
    HXFR_HS = 1u << 7,
};

enum
{
    HRSL_RESULT_MASK = 0x0f,
    HRSL_RCVTOGRD = 1u << 4,
    HRSL_SNDTOGRD = 1u << 5,
    HRSL_KSTATUS = 1u << 6,
    HRSL_JSTATUS = 1u << 7,
};

enum
{
    HRSL_SUCCESS = 0,
    HRSL_BUSY,
    HRSL_BAD_REQ,
    HRSL_UNDEF,
    HRSL_NAK,
    HRSL_STALL,
    HRSL_TOG_ERR,
    HRSL_WRONG_PID,
    HRSL_BAD_BYTECOUNT,
    HRSL_PID_ERR,
    HRSL_PKT_ERR,
    HRSL_CRC_ERR,
    HRSL_K_ERR,
    HRSL_J_ERR,
    HRSL_TIMEOUT,
    HRSL_BABBLE,
};

enum
{
    DEFAULT_HIEN = HIRQ_CONDET_IRQ | HIRQ_FRAME_IRQ | HIRQ_HXFRDN_IRQ | HIRQ_RCVDAV_IRQ
};

enum
{
    MAX_NAK_DEFAULT = 1 // Number of NAK per endpoint per usb frame
};

enum
{
    EP_STATE_IDLE = 0,
    EP_STATE_COMPLETE = 1,
    EP_STATE_ABORTING = 2,
    EP_STATE_ATTEMPT_1 = 3, // pending 1st attempt
    EP_STATE_ATTEMPT_MAX = 15
};

//--------------------------------------------------------------------+
//
//--------------------------------------------------------------------+

typedef struct
{
    uint8_t daddr;

    union
    {
        ;
        struct TU_ATTR_PACKED
        {
            uint8_t ep_num : 4;
            uint8_t is_setup : 1;
            uint8_t is_out : 1;
            uint8_t is_iso : 1;
        } hxfr_bm;

        uint8_t hxfr;
    };

    struct TU_ATTR_PACKED
    {
        uint8_t state : 4;
        uint8_t data_toggle : 1;
        uint16_t packet_size : 11;
    };
    int retry_count;
    uint16_t total_len;
    uint16_t xferred_len;
    uint8_t *buf;
} max3421_ep_t;

TU_VERIFY_STATIC(sizeof(max3421_ep_t) == 16, "size is not correct");

typedef struct
{
    volatile uint16_t frame_count;

    // cached register
    uint8_t sndbc;
    uint8_t hirq;
    uint8_t hien;
    uint8_t mode;
    uint8_t peraddr;
    uint8_t hxfr;

    atomic_flag busy; // busy transferring

#if OSAL_MUTEX_REQUIRED
    OSAL_MUTEX_DEF(spi_mutexdef);
    osal_mutex_t spi_mutex;
#endif

    max3421_ep_t ep[CFG_TUH_MAX3421_ENDPOINT_TOTAL]; // [0] is reserved for addr0
} max3421_data_t;




EXTERN_C_BEGIN
void print_endpoint_status(void);
void log_debug_data();
void log_action(const char *format, ...);
EXTERN_C_END