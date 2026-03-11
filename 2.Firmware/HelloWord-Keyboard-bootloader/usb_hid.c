#include "usb_hid.h"
#include "stm32f1xx.h"

/* ---- STM32F103 USB Peripheral ---- */
#define USB_BASE        0x40005C00U
#define USB_PMA_BASE    0x40006000U

#define USB_EPR(ep)     (*(volatile uint16_t *)(USB_BASE + (uint32_t)(ep) * 4U))
#define USB_CNTR        (*(volatile uint16_t *)(USB_BASE + 0x40U))
#define USB_ISTR        (*(volatile uint16_t *)(USB_BASE + 0x44U))
#define USB_DADDR       (*(volatile uint16_t *)(USB_BASE + 0x4CU))
#define USB_BTABLE      (*(volatile uint16_t *)(USB_BASE + 0x50U))
#define USB_PMA(off)    (*(volatile uint16_t *)(USB_PMA_BASE + ((uint32_t)(off) << 1)))

#define ISTR_CTR        0x8000U
#define ISTR_RESET      0x0400U

#define EP_CTR_RX       0x8000U
#define EP_DTOG_RX      0x4000U
#define EP_STAT_RX      0x3000U
#define EP_SETUP        0x0800U
#define EP_TYPE         0x0600U
#define EP_KIND         0x0100U
#define EP_CTR_TX       0x0080U
#define EP_DTOG_TX      0x0040U
#define EP_STAT_TX      0x0030U
#define EP_EA           0x000FU

#define EP_TX_STALL     0x0010U
#define EP_TX_NAK       0x0020U
#define EP_TX_VALID     0x0030U
#define EP_RX_VALID     0x3000U

#define EP_TYPE_CONTROL     0x0200U
#define EP_TYPE_INTERRUPT   0x0600U
#define EP_RW_MASK      (EP_CTR_RX | EP_TYPE | EP_KIND | EP_CTR_TX | EP_EA)

/* PMA buffer layout */
#define EP0_TX_PMA      0x40U
#define EP0_RX_PMA      0x80U
#define EP1_TX_PMA      0xC0U
#define EP1_RX_PMA      0x100U
#define MAX_PKT         64U

/* BDT offsets per EP (8 bytes each in PMA) */
#define BDT_TX_ADDR     0U
#define BDT_TX_COUNT    2U
#define BDT_RX_ADDR     4U
#define BDT_RX_COUNT    6U
#define EP0_BDT         0U
#define EP1_BDT         8U

/* BL_SIZE=1, NUM_BLOCK=2 → 2×32 = 64 bytes */
#define RX_BUF_64       0x8400U

/* USB descriptor types */
#define DESC_DEVICE             1U
#define DESC_CONFIGURATION      2U
#define DESC_STRING             3U
#define DESC_INTERFACE          4U
#define DESC_ENDPOINT           5U
#define DESC_DEVICE_QUALIFIER   6U
#define DESC_HID                0x21U
#define DESC_HID_REPORT         0x22U

/* USB standard requests */
#define REQ_GET_STATUS          0U
#define REQ_CLEAR_FEATURE       1U
#define REQ_SET_ADDRESS         5U
#define REQ_GET_DESCRIPTOR      6U
#define REQ_GET_CONFIGURATION   8U
#define REQ_SET_CONFIGURATION   9U
#define REQ_GET_INTERFACE       10U
#define REQ_SET_INTERFACE       11U

/* HID class requests */
#define HID_SET_IDLE            0x0AU

/* Bootloader protocol */
#define CMD_INFO        0x01U
#define CMD_ERASE       0x02U
#define CMD_WRITE       0x03U
#define CMD_SEAL        0x04U
#define CMD_REBOOT      0x05U

#define RSP_OK          0x00U
#define RSP_ERR         0x01U
#define RSP_BAD_STATE   0x02U
#define RSP_BAD_ADDR    0x03U
#define RSP_CRC_FAIL    0x04U

#define BL_VER_MAJOR    1U
#define BL_VER_MINOR    0U
#define WRITE_DATA_LEN  60U

#define USB_VID         0x1001U
#define USB_PID         0xB007U
#define LANGID_EN       0x0409U

/* ---- Types ---- */
typedef struct __attribute__((packed))
{
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_t;

typedef enum { CTRL_IDLE = 0, CTRL_DATA_IN, CTRL_STATUS_IN } ctrl_stage_t;

typedef struct
{
    usb_setup_t setup;
    ctrl_stage_t ctrl_stage;
    const uint8_t *ep0_tx_ptr;
    uint16_t ep0_tx_rem;
    uint8_t  pending_addr;
    uint8_t  configured;

    uint8_t  rx_buf[64];
    uint8_t  resp[64];

    uint8_t  erase_active;
    uint32_t erase_addr;

    uint8_t  flash_ready;
    uint8_t  complete;
} ctx_t;

static ctx_t g;
static uint8_t str_buf[64];

/* ---- USB Descriptors ---- */
static const uint8_t hid_report_desc[] = {
    0x06, 0x00, 0xFF,
    0x09, 0x01,
    0xA1, 0x01,
    0x09, 0x01, 0x15, 0x00, 0x26, 0xFF, 0x00,
    0x75, 0x08, 0x95, 0x40, 0x81, 0x02,
    0x09, 0x02, 0x15, 0x00, 0x26, 0xFF, 0x00,
    0x75, 0x08, 0x95, 0x40, 0x91, 0x02,
    0xC0
};

#define HID_RPT_SIZE    ((uint16_t)sizeof(hid_report_desc))
#define CFG_TOTAL       41U

static const uint8_t dev_desc[] = {
    18, DESC_DEVICE,
    0x00, 0x02,
    0x00, 0x00, 0x00,
    MAX_PKT,
    USB_VID & 0xFF, USB_VID >> 8,
    USB_PID & 0xFF, USB_PID >> 8,
    BL_VER_MINOR, BL_VER_MAJOR,
    1, 2, 3,
    1
};

static const uint8_t cfg_desc[] = {
    9, DESC_CONFIGURATION,
    CFG_TOTAL, 0x00,
    1, 1, 0,
    0x80, 50,

    9, DESC_INTERFACE,
    0, 0,
    2,
    0x03, 0x00, 0x00,
    0,

    9, DESC_HID,
    0x11, 0x01,
    0x00, 1,
    DESC_HID_REPORT,
    HID_RPT_SIZE & 0xFF, HID_RPT_SIZE >> 8,

    7, DESC_ENDPOINT,
    0x81, 0x03,
    MAX_PKT, 0x00,
    1,

    7, DESC_ENDPOINT,
    0x01, 0x03,
    MAX_PKT, 0x00,
    1
};

static const uint8_t hid_desc[] = {
    9, DESC_HID,
    0x11, 0x01, 0x00, 1, DESC_HID_REPORT,
    HID_RPT_SIZE & 0xFF, HID_RPT_SIZE >> 8
};

static const uint8_t lang_desc[] = {
    4, DESC_STRING, LANGID_EN & 0xFF, LANGID_EN >> 8
};

/* ---- Low-level USB helpers ---- */
static void ep_set_tx(uint8_t ep, uint16_t st)
{
    uint16_t v = USB_EPR(ep);
    USB_EPR(ep) = (v & EP_RW_MASK) | ((v ^ st) & EP_STAT_TX);
}

static void ep_set_rx(uint8_t ep, uint16_t st)
{
    uint16_t v = USB_EPR(ep);
    USB_EPR(ep) = (v & EP_RW_MASK) | ((v ^ st) & EP_STAT_RX);
}

static void ep_clr_ctr_rx(uint8_t ep)
{
    uint16_t v = USB_EPR(ep);
    USB_EPR(ep) = (v & EP_RW_MASK) & (uint16_t)~EP_CTR_RX;
}

static void ep_clr_ctr_tx(uint8_t ep)
{
    uint16_t v = USB_EPR(ep);
    USB_EPR(ep) = (v & EP_RW_MASK) & (uint16_t)~EP_CTR_TX;
}

static void ep_clr_dtog(uint8_t ep)
{
    uint16_t v = USB_EPR(ep);
    uint16_t t = v & (EP_DTOG_RX | EP_DTOG_TX);
    if (t)
        USB_EPR(ep) = (v & EP_RW_MASK) | t;
}

static void pma_write(uint16_t off, const uint8_t *buf, uint16_t len)
{
    for (uint16_t i = 0; i < (uint16_t)((len + 1U) / 2U); i++)
    {
        uint16_t w = buf[i * 2U];
        if ((uint16_t)(i * 2U + 1U) < len)
            w |= (uint16_t)buf[i * 2U + 1U] << 8;
        USB_PMA(off) = w;
        off += 2U;
    }
}

static void pma_read(uint16_t off, uint8_t *buf, uint16_t len)
{
    for (uint16_t i = 0; i < (uint16_t)((len + 1U) / 2U); i++)
    {
        uint16_t w = USB_PMA(off);
        buf[i * 2U] = (uint8_t)(w & 0xFFU);
        if ((uint16_t)(i * 2U + 1U) < len)
            buf[i * 2U + 1U] = (uint8_t)(w >> 8);
        off += 2U;
    }
}

/* ---- EP0 control transfer helpers ---- */
static void ep0_tx_pkt(const uint8_t *data, uint16_t len)
{
    if (len > MAX_PKT) len = MAX_PKT;
    pma_write(EP0_TX_PMA, data, len);
    USB_PMA(EP0_BDT + BDT_TX_COUNT) = len;
    ep_set_tx(0, EP_TX_VALID);
}

static void ep0_send(const uint8_t *data, uint16_t len)
{
    uint16_t chunk = (len > MAX_PKT) ? MAX_PKT : len;
    g.ep0_tx_ptr = data + chunk;
    g.ep0_tx_rem = len - chunk;
    ep0_tx_pkt(data, chunk);
    g.ctrl_stage = (len > chunk) ? CTRL_DATA_IN : CTRL_STATUS_IN;
}

static void ep0_zlp(void)
{
    g.ep0_tx_ptr = 0;
    g.ep0_tx_rem = 0;
    ep0_tx_pkt(0, 0);
    g.ctrl_stage = CTRL_STATUS_IN;
}

static void ep0_stall(void)
{
    ep_set_tx(0, EP_TX_STALL);
    ep_set_rx(0, EP_RX_VALID);
}

/* ---- String descriptors ---- */
static const uint8_t *get_string(uint8_t idx, uint16_t *len)
{
    const char *s = 0;
    uint16_t slen = 0, total;

    if (idx == 0) { *len = sizeof(lang_desc); return lang_desc; }

    switch (idx)
    {
        case 1: s = "HelloWord"; break;
        case 2: s = "HW75 Bootloader"; break;
        case 3: s = "HW75-BL-HID"; break;
        default: *len = 0; return 0;
    }

    while (s[slen]) slen++;
    total = (uint16_t)(2U + slen * 2U);
    if (total > sizeof(str_buf)) total = sizeof(str_buf);

    str_buf[0] = (uint8_t)total;
    str_buf[1] = DESC_STRING;
    for (uint16_t i = 0; i < slen && (uint16_t)(2U + i * 2U + 1U) < total; i++)
    {
        str_buf[2U + i * 2U] = (uint8_t)s[i];
        str_buf[2U + i * 2U + 1U] = 0;
    }
    *len = total;
    return str_buf;
}

/* ---- EP0 setup handling ---- */
static void handle_get_descriptor(void)
{
    const uint8_t *d = 0;
    uint16_t dlen = 0;
    uint8_t type = (uint8_t)(g.setup.wValue >> 8);

    switch (type)
    {
        case DESC_DEVICE:
            d = dev_desc; dlen = sizeof(dev_desc); break;
        case DESC_CONFIGURATION:
            d = cfg_desc; dlen = sizeof(cfg_desc); break;
        case DESC_STRING:
            d = get_string((uint8_t)(g.setup.wValue & 0xFF), &dlen); break;
        case DESC_HID:
            d = hid_desc; dlen = sizeof(hid_desc); break;
        case DESC_HID_REPORT:
            d = hid_report_desc; dlen = sizeof(hid_report_desc); break;
        default:
            ep0_stall(); return;
    }

    if (!d) { ep0_stall(); return; }
    if (dlen > g.setup.wLength) dlen = g.setup.wLength;
    ep0_send(d, dlen);
}

static void handle_setup(void)
{
    uint8_t reqtype = g.setup.bmRequestType & 0x60U;

    if (reqtype == 0x00U)
    {
        switch (g.setup.bRequest)
        {
            case REQ_GET_DESCRIPTOR:    handle_get_descriptor(); break;
            case REQ_SET_ADDRESS:
                g.pending_addr = (uint8_t)(g.setup.wValue & 0x7FU);
                ep0_zlp();
                break;
            case REQ_SET_CONFIGURATION:
                g.configured = (g.setup.wValue == 1) ? 1 : 0;
                ep0_zlp();
                break;
            case REQ_GET_CONFIGURATION:
            {
                uint8_t c = g.configured ? 1 : 0;
                ep0_send(&c, 1);
                break;
            }
            case REQ_GET_STATUS:
            {
                static const uint8_t st[2] = {0, 0};
                ep0_send(st, 2);
                break;
            }
            case REQ_GET_INTERFACE:
            {
                static const uint8_t z = 0;
                ep0_send(&z, 1);
                break;
            }
            case REQ_SET_INTERFACE:
            case REQ_CLEAR_FEATURE:
                ep0_zlp(); break;
            default:
                ep0_stall(); break;
        }
    }
    else if (reqtype == 0x20U)
    {
        if (g.setup.bRequest == HID_SET_IDLE)
            ep0_zlp();
        else
            ep0_stall();
    }
    else
    {
        ep0_stall();
    }
}

static void ep0_tx_done(void)
{
    if (g.pending_addr)
    {
        USB_DADDR = 0x0080U | g.pending_addr;
        g.pending_addr = 0;
    }

    if (g.ctrl_stage == CTRL_DATA_IN && g.ep0_tx_rem > 0)
    {
        uint16_t chunk = (g.ep0_tx_rem > MAX_PKT) ? MAX_PKT : g.ep0_tx_rem;
        ep0_tx_pkt(g.ep0_tx_ptr, chunk);
        g.ep0_tx_ptr += chunk;
        g.ep0_tx_rem -= chunk;
        return;
    }

    g.ctrl_stage = CTRL_IDLE;
    ep_set_rx(0, EP_RX_VALID);
}

/* ---- Flash operations ---- */
static int flash_wait(void)
{
    while (FLASH->SR & FLASH_SR_BSY) {}
    return (FLASH->SR & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) ? -1 : 0;
}

static void flash_unlock(void)
{
    if (FLASH->CR & FLASH_CR_LOCK)
    {
        FLASH->KEYR = 0x45670123U;
        FLASH->KEYR = 0xCDEF89ABU;
    }
}

static void flash_lock(void)
{
    FLASH->CR |= FLASH_CR_LOCK;
}

static int flash_erase_page(uint32_t addr)
{
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR;
    if (flash_wait() != 0) return -1;

    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR = addr;
    FLASH->CR |= FLASH_CR_STRT;

    int rc = flash_wait();
    FLASH->CR &= ~FLASH_CR_PER;
    return rc;
}

static int flash_write_data(uint32_t addr, const uint8_t *data, uint16_t len)
{
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR;
    if (flash_wait() != 0) return -1;

    FLASH->CR |= FLASH_CR_PG;
    for (uint16_t i = 0; i < len; i += 2U)
    {
        uint16_t hw = data[i];
        if ((uint16_t)(i + 1U) < len)
            hw |= (uint16_t)data[i + 1U] << 8;
        else
            hw |= 0xFF00U;

        *(volatile uint16_t *)(addr + i) = hw;
        if (flash_wait() != 0)
        {
            FLASH->CR &= ~FLASH_CR_PG;
            return -1;
        }
    }
    FLASH->CR &= ~FLASH_CR_PG;
    return 0;
}

/* ---- CRC32 (matches Python binascii.crc32) ---- */
static uint32_t crc32_compute(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFU;
    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
            crc = (crc >> 1) ^ ((crc & 1U) ? 0xEDB88320U : 0U);
    }
    return crc ^ 0xFFFFFFFFU;
}

/* ---- Helpers ---- */
static void store_le32(uint8_t *buf, uint32_t v)
{
    buf[0] = (uint8_t)(v);
    buf[1] = (uint8_t)(v >> 8);
    buf[2] = (uint8_t)(v >> 16);
    buf[3] = (uint8_t)(v >> 24);
}

static uint32_t load_le32(const uint8_t *buf)
{
    return (uint32_t)buf[0]
         | ((uint32_t)buf[1] << 8)
         | ((uint32_t)buf[2] << 16)
         | ((uint32_t)buf[3] << 24);
}

static uint32_t load_le24(const uint8_t *buf)
{
    return (uint32_t)buf[0]
         | ((uint32_t)buf[1] << 8)
         | ((uint32_t)buf[2] << 16);
}

static void resp_clear(void)
{
    for (int i = 0; i < 64; i++) g.resp[i] = 0;
}

/* ---- EP1 send response ---- */
static void ep1_send(void)
{
    pma_write(EP1_TX_PMA, g.resp, 64U);
    USB_PMA(EP1_BDT + BDT_TX_COUNT) = 64U;
    ep_set_tx(1, EP_TX_VALID);
}

/* ---- Non-blocking flash erase ---- */
static void erase_finish(uint8_t status)
{
    flash_lock();
    g.erase_active = 0;
    g.flash_ready = (status == RSP_OK);

    resp_clear();
    g.resp[0] = CMD_ERASE;
    g.resp[1] = status;
    ep1_send();
    ep_set_rx(1, EP_RX_VALID);
}

static void erase_step(void)
{
    if (g.erase_addr == APP_ADDRESS)
        flash_unlock();

    if (flash_erase_page(g.erase_addr) != 0)
    {
        erase_finish(RSP_ERR);
        return;
    }

    g.erase_addr += FLASH_PAGE_SIZE;
    if (g.erase_addr >= FLASH_END_ADDRESS)
        erase_finish(RSP_OK);
}

/* ---- Protocol command processing ---- */
static void process_cmd(void)
{
    resp_clear();
    uint8_t cmd = g.rx_buf[0];
    g.resp[0] = cmd;

    switch (cmd)
    {
    case CMD_INFO:
        store_le32(&g.resp[1], APP_SIZE);
        g.resp[5] = (uint8_t)(FLASH_PAGE_SIZE & 0xFF);
        g.resp[6] = (uint8_t)(FLASH_PAGE_SIZE >> 8);
        g.resp[7] = BL_VER_MAJOR;
        g.resp[8] = BL_VER_MINOR;
        ep1_send();
        break;

    case CMD_ERASE:
        if (g.erase_active)
        {
            g.resp[1] = RSP_BAD_STATE;
            ep1_send();
            break;
        }
        g.erase_active = 1;
        g.erase_addr = APP_ADDRESS;
        g.flash_ready = 0;
        return; /* RX stays NAKed; response deferred until erase completes */

    case CMD_WRITE:
    {
        if (!g.flash_ready)
        {
            g.resp[1] = RSP_BAD_STATE;
            ep1_send();
            break;
        }

        uint32_t offset = load_le24(&g.rx_buf[1]);
        uint32_t addr = APP_ADDRESS + offset;
        uint16_t len = WRITE_DATA_LEN;

        if (addr >= FLASH_END_ADDRESS)
        {
            g.resp[1] = RSP_BAD_ADDR;
            ep1_send();
            break;
        }
        if (addr + len > FLASH_END_ADDRESS)
            len = (uint16_t)(FLASH_END_ADDRESS - addr);

        flash_unlock();
        int rc = flash_write_data(addr, &g.rx_buf[4], len);
        flash_lock();

        g.resp[1] = (rc == 0) ? RSP_OK : RSP_ERR;
        ep1_send();
        break;
    }

    case CMD_SEAL:
    {
        uint32_t size = load_le32(&g.rx_buf[1]);
        uint32_t expected = load_le32(&g.rx_buf[5]);

        if (size > APP_SIZE)
        {
            g.resp[1] = RSP_BAD_ADDR;
            ep1_send();
            break;
        }

        uint32_t actual = crc32_compute((const uint8_t *)APP_ADDRESS, size);
        g.resp[1] = (actual == expected) ? RSP_OK : RSP_CRC_FAIL;
        ep1_send();
        break;
    }

    case CMD_REBOOT:
        g.resp[1] = RSP_OK;
        ep1_send();
        g.complete = 1;
        break;

    default:
        g.resp[1] = RSP_ERR;
        ep1_send();
        break;
    }

    ep_set_rx(1, EP_RX_VALID);
}

/* ---- EP0/EP1 event handlers ---- */
static void handle_ep0(void)
{
    uint16_t epr = USB_EPR(0);

    if (epr & EP_SETUP)
    {
        ep_clr_ctr_rx(0);
        pma_read(EP0_RX_PMA, (uint8_t *)&g.setup, sizeof(g.setup));
        handle_setup();
    }
    else if (epr & EP_CTR_RX)
    {
        ep_clr_ctr_rx(0);
        ep_set_rx(0, EP_RX_VALID);
    }

    if (epr & EP_CTR_TX)
    {
        ep_clr_ctr_tx(0);
        ep0_tx_done();
    }
}

static void handle_ep1(void)
{
    uint16_t epr = USB_EPR(1);

    if (epr & EP_CTR_RX)
    {
        ep_clr_ctr_rx(1);
        uint16_t count = USB_PMA(EP1_BDT + BDT_RX_COUNT) & 0x03FFU;
        if (count == 64U)
        {
            pma_read(EP1_RX_PMA, g.rx_buf, 64U);
            process_cmd();
        }
        else
        {
            ep_set_rx(1, EP_RX_VALID);
        }
    }

    if (epr & EP_CTR_TX)
        ep_clr_ctr_tx(1);
}

/* ---- USB reset ---- */
static void usb_reset(void)
{
    USB_BTABLE = 0;

    USB_PMA(EP0_BDT + BDT_TX_ADDR)  = EP0_TX_PMA;
    USB_PMA(EP0_BDT + BDT_TX_COUNT) = 0;
    USB_PMA(EP0_BDT + BDT_RX_ADDR)  = EP0_RX_PMA;
    USB_PMA(EP0_BDT + BDT_RX_COUNT) = RX_BUF_64;

    USB_PMA(EP1_BDT + BDT_TX_ADDR)  = EP1_TX_PMA;
    USB_PMA(EP1_BDT + BDT_TX_COUNT) = 0;
    USB_PMA(EP1_BDT + BDT_RX_ADDR)  = EP1_RX_PMA;
    USB_PMA(EP1_BDT + BDT_RX_COUNT) = RX_BUF_64;

    USB_EPR(0) = EP_TYPE_CONTROL;
    ep_clr_dtog(0);
    ep_set_tx(0, EP_TX_NAK);
    ep_set_rx(0, EP_RX_VALID);

    USB_EPR(1) = EP_TYPE_INTERRUPT | 1U;
    ep_clr_dtog(1);
    ep_set_tx(1, EP_TX_NAK);
    ep_set_rx(1, EP_RX_VALID);

    USB_DADDR = 0x0080U;

    g.configured = 0;
    g.ctrl_stage = CTRL_IDLE;
    g.pending_addr = 0;
    g.ep0_tx_ptr = 0;
    g.ep0_tx_rem = 0;
}

/* ---- Public API ---- */
void usb_hid_init(void)
{
    g.complete = 0;
    g.erase_active = 0;
    g.flash_ready = 0;

    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    GPIOA->CRH = (GPIOA->CRH & ~(0xFU << 16)) | (0x2U << 16);
    GPIOA->BRR = (1U << 12);
    for (volatile int i = 0; i < 200000; i++) {}

    GPIOA->CRH = (GPIOA->CRH & ~(0xFU << 16)) | (0x4U << 16);

    RCC->APB1ENR |= RCC_APB1ENR_USBEN;
    USB_CNTR = 0x0003U;
    for (volatile int i = 0; i < 100; i++) {}
    USB_CNTR = 0x0001U;
    for (volatile int i = 0; i < 100; i++) {}
    USB_CNTR = 0x0000U;
    USB_ISTR = 0;

    usb_reset();
    USB_CNTR = 0x8400U;
}

void usb_hid_poll(void)
{
    uint16_t istr = USB_ISTR;

    if (istr & ISTR_RESET)
    {
        USB_ISTR = (uint16_t)~ISTR_RESET;
        usb_reset();
        return;
    }

    if (istr & ISTR_CTR)
    {
        uint8_t ep_id = (uint8_t)(istr & 0x0FU);
        if (ep_id == 0)
            handle_ep0();
        else if (ep_id == 1)
            handle_ep1();
    }

    if (g.erase_active)
        erase_step();
}

int usb_hid_is_complete(void)
{
    return g.complete;
}
