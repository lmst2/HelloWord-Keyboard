#include "usb_dfu.h"
#include "stm32f1xx.h"
#include <string.h>

/* ---- STM32F103 USB Peripheral Registers ---- */
#define USB_BASE        0x40005C00U
#define USB_PMA_BASE    0x40006000U

#define USB_EPR(n)      (*(volatile uint16_t*)(USB_BASE + (n) * 4U))
#define USB_CNTR        (*(volatile uint16_t*)(USB_BASE + 0x40U))
#define USB_ISTR        (*(volatile uint16_t*)(USB_BASE + 0x44U))
#define USB_DADDR       (*(volatile uint16_t*)(USB_BASE + 0x4CU))
#define USB_BTABLE      (*(volatile uint16_t*)(USB_BASE + 0x50U))

/* PMA access: each 16-bit PMA word is at a 32-bit-aligned address */
#define PMA(offset)     (*(volatile uint16_t*)(USB_PMA_BASE + ((uint32_t)(offset) << 1)))

/* EP register bits */
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

#define EP_TX_DIS       0x0000U
#define EP_TX_STALL     0x0010U
#define EP_TX_NAK       0x0020U
#define EP_TX_VALID     0x0030U
#define EP_RX_DIS       0x0000U
#define EP_RX_STALL     0x1000U
#define EP_RX_NAK       0x2000U
#define EP_RX_VALID     0x3000U

#define EP_TYPE_CONTROL 0x0200U

/* Invariant mask: bits to preserve when writing (CTR_RX, TYPE, KIND, CTR_TX, EA) */
#define EP_RW_MASK      (EP_CTR_RX | EP_TYPE | EP_KIND | EP_CTR_TX | EP_EA)

/* PMA Buffer Descriptor Table layout for EP0 (at PMA offset 0) */
#define BDT_ADDR_TX     0
#define BDT_COUNT_TX    2
#define BDT_ADDR_RX     4
#define BDT_COUNT_RX    6

/* PMA buffer addresses */
#define EP0_TX_ADDR     0x40U
#define EP0_RX_ADDR     0x80U
#define EP0_MAX_PKT     64U

/* ---- EP register helpers ---- */
static void ep_set_tx_status(uint8_t ep, uint16_t status)
{
    uint16_t val = USB_EPR(ep);
    USB_EPR(ep) = (val & EP_RW_MASK) | ((val ^ status) & EP_STAT_TX);
}

static void ep_set_rx_status(uint8_t ep, uint16_t status)
{
    uint16_t val = USB_EPR(ep);
    USB_EPR(ep) = (val & EP_RW_MASK) | ((val ^ status) & EP_STAT_RX);
}

static void ep_clear_ctr_rx(uint8_t ep)
{
    uint16_t val = USB_EPR(ep);
    USB_EPR(ep) = (val & EP_RW_MASK) & ~EP_CTR_RX;
}

static void ep_clear_ctr_tx(uint8_t ep)
{
    uint16_t val = USB_EPR(ep);
    USB_EPR(ep) = (val & EP_RW_MASK) & ~EP_CTR_TX;
}

/* ---- PMA data helpers ---- */
static void pma_write(uint16_t pma_off, const uint8_t *buf, uint16_t len)
{
    for (uint16_t i = 0; i < (len + 1) / 2; i++)
    {
        uint16_t val = buf[i * 2];
        if (i * 2 + 1 < len) val |= (uint16_t)buf[i * 2 + 1] << 8;
        PMA(pma_off) = val;
        pma_off += 2;
    }
}

static void pma_read(uint16_t pma_off, uint8_t *buf, uint16_t len)
{
    for (uint16_t i = 0; i < (len + 1) / 2; i++)
    {
        uint16_t val = PMA(pma_off);
        buf[i * 2] = val & 0xFF;
        if (i * 2 + 1 < len) buf[i * 2 + 1] = (val >> 8) & 0xFF;
        pma_off += 2;
    }
}

static void ep0_tx(const uint8_t *data, uint16_t len)
{
    if (len > EP0_MAX_PKT) len = EP0_MAX_PKT;
    pma_write(EP0_TX_ADDR, data, len);
    PMA(BDT_COUNT_TX) = len;
    ep_set_tx_status(0, EP_TX_VALID);
}

static void ep0_tx_stall(void)
{
    ep_set_tx_status(0, EP_TX_STALL);
    ep_set_rx_status(0, EP_RX_VALID);
}

/* ---- USB Descriptors ---- */
static const uint8_t dev_desc[] = {
    18,                     /* bLength */
    USB_DESC_DEVICE,        /* bDescriptorType */
    0x00, 0x02,             /* bcdUSB = 2.00 */
    0x00,                   /* bDeviceClass */
    0x00,                   /* bDeviceSubClass */
    0x00,                   /* bDeviceProtocol */
    EP0_MAX_PKT,            /* bMaxPacketSize0 */
    USB_VID & 0xFF, USB_VID >> 8,
    USB_PID & 0xFF, USB_PID >> 8,
    0x00, 0x02,             /* bcdDevice = 2.00 */
    1,                      /* iManufacturer */
    2,                      /* iProduct */
    3,                      /* iSerialNumber */
    1                       /* bNumConfigurations */
};

static const uint8_t cfg_desc[] = {
    /* Configuration Descriptor */
    9, USB_DESC_CONFIGURATION,
    27, 0,                  /* wTotalLength = 9+9+9 */
    1,                      /* bNumInterfaces */
    1,                      /* bConfigurationValue */
    0,                      /* iConfiguration */
    0x80,                   /* bmAttributes: bus powered */
    50,                     /* bMaxPower: 100mA */

    /* Interface Descriptor */
    9, USB_DESC_INTERFACE,
    0,                      /* bInterfaceNumber */
    0,                      /* bAlternateSetting */
    0,                      /* bNumEndpoints (DFU uses only EP0) */
    0xFE,                   /* bInterfaceClass: Application Specific */
    0x01,                   /* bInterfaceSubClass: DFU */
    0x02,                   /* bInterfaceProtocol: DFU Mode */
    4,                      /* iInterface */

    /* DFU Functional Descriptor */
    9, USB_DESC_DFU_FUNCTIONAL,
    0x09,                   /* bmAttributes: canDnload | willDetach */
    0xFF, 0x00,             /* wDetachTimeout: 255ms */
    DFU_XFER_SIZE & 0xFF, DFU_XFER_SIZE >> 8,
    0x1A, 0x01              /* bcdDFUVersion: 1.1a */
};

/* String 0: Language ID */
static const uint8_t str0[] = {4, USB_DESC_STRING, 0x09, 0x04};
/* Strings 1-4 encoded as UTF-16LE inline in get_string_desc() */

static uint8_t str_buf[64];

static const uint8_t *get_string_desc(uint8_t index, uint16_t *len)
{
    const char *s = 0;
    switch (index)
    {
        case 0: *len = sizeof(str0); return str0;
        case 1: s = "STMicroelectronics"; break;
        case 2: s = "HelloWord Keyboard DFU"; break;
        case 3: s = "HW75-BL"; break;
        case 4: s = "@Internal Flash /0x08004000/112*001Kg"; break;
        default: *len = 0; return 0;
    }
    uint16_t slen = 0;
    while (s[slen]) slen++;
    uint16_t total = 2 + slen * 2;
    if (total > sizeof(str_buf)) total = sizeof(str_buf);
    str_buf[0] = total;
    str_buf[1] = USB_DESC_STRING;
    for (uint16_t i = 0; i < slen && (2 + i * 2 + 1) < sizeof(str_buf); i++)
    {
        str_buf[2 + i * 2] = s[i];
        str_buf[2 + i * 2 + 1] = 0;
    }
    *len = total;
    return str_buf;
}

/* ---- Flash operations ---- */
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
    while (FLASH->SR & FLASH_SR_BSY);

    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR = addr;
    FLASH->CR |= FLASH_CR_STRT;

    while (FLASH->SR & FLASH_SR_BSY);

    FLASH->CR &= ~FLASH_CR_PER;
    return (FLASH->SR & FLASH_SR_WRPRTERR) ? -1 : 0;
}

static int flash_write(uint32_t addr, const uint8_t *data, uint16_t len)
{
    flash_unlock();

    FLASH->CR |= FLASH_CR_PG;

    for (uint16_t i = 0; i < len; i += 2)
    {
        uint16_t hw = data[i];
        if (i + 1 < len) hw |= (uint16_t)data[i + 1] << 8;
        else hw |= 0xFF00U;

        *(volatile uint16_t*)(addr + i) = hw;
        while (FLASH->SR & FLASH_SR_BSY);

        if (FLASH->SR & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR))
        {
            FLASH->CR &= ~FLASH_CR_PG;
            flash_lock();
            return -1;
        }
    }

    FLASH->CR &= ~FLASH_CR_PG;
    flash_lock();
    return 0;
}

/* ---- DFU State Machine ---- */
static dfu_state_t  dfu_state;
static dfu_status_t dfu_status;
static uint16_t     dfu_block_num;
static uint16_t     dfu_data_len;
static uint8_t      dfu_buffer[DFU_XFER_SIZE];
static uint16_t     dfu_buf_offset;
static volatile int dfu_complete_flag;
static uint8_t      pending_addr;

/* Multi-packet control OUT accumulation */
static uint16_t     ctrl_total_len;
static uint16_t     ctrl_received;

/* EP0 control transfer state */
typedef enum { CTRL_IDLE, CTRL_DATA_OUT, CTRL_STATUS_IN, CTRL_DATA_IN } ctrl_stage_t;
static ctrl_stage_t ctrl_stage;

/* Transmit pointer for multi-packet IN */
static const uint8_t *tx_ptr;
static uint16_t       tx_remaining;

/* ---- Setup packet ---- */
typedef struct __attribute__((packed)) {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_t;

static usb_setup_t setup_pkt;

/* ---- DFU Request Handlers ---- */
static void dfu_handle_dnload(void)
{
    dfu_block_num = setup_pkt.wValue;
    ctrl_total_len = setup_pkt.wLength;
    ctrl_received = 0;

    if (ctrl_total_len == 0)
    {
        dfu_state = STATE_DFU_MANIFEST_SYNC;
        ep0_tx(0, 0);
        ctrl_stage = CTRL_IDLE;
        return;
    }

    if (ctrl_total_len > DFU_XFER_SIZE)
    {
        dfu_state = STATE_DFU_ERROR;
        dfu_status = STATUS_ERR_ADDRESS;
        ep0_tx_stall();
        return;
    }

    /* Prepare to receive data packets */
    dfu_buf_offset = 0;
    ctrl_stage = CTRL_DATA_OUT;
    ep_set_rx_status(0, EP_RX_VALID);
}

static void dfu_handle_getstatus(void)
{
    uint8_t resp[6];

    if (dfu_state == STATE_DFU_DNLOAD_SYNC)
    {
        /* Erase page and write data */
        uint32_t addr = APP_ADDRESS + (uint32_t)dfu_block_num * DFU_XFER_SIZE;
        uint32_t flash_end = APP_ADDRESS + 0x1C000U; /* 112KB */

        if (addr >= flash_end || addr + dfu_data_len > flash_end)
        {
            dfu_state = STATE_DFU_ERROR;
            dfu_status = STATUS_ERR_ADDRESS;
        }
        else
        {
            flash_unlock();
            if (flash_erase_page(addr) != 0)
            {
                dfu_state = STATE_DFU_ERROR;
                dfu_status = STATUS_ERR_ERASE;
            }
            else if (flash_write(addr, dfu_buffer, dfu_data_len) != 0)
            {
                dfu_state = STATE_DFU_ERROR;
                dfu_status = STATUS_ERR_WRITE;
            }
            else
            {
                dfu_state = STATE_DFU_DNLOAD_IDLE;
                dfu_status = STATUS_OK;
            }
            flash_lock();
        }
    }
    else if (dfu_state == STATE_DFU_MANIFEST_SYNC)
    {
        dfu_complete_flag = 1;
        dfu_state = STATE_DFU_IDLE;
    }

    resp[0] = dfu_status;
    resp[1] = 10; resp[2] = 0; resp[3] = 0; /* bwPollTimeout = 10ms */
    resp[4] = dfu_state;
    resp[5] = 0;

    ep0_tx(resp, 6);
    ctrl_stage = CTRL_STATUS_IN;
}

static void dfu_handle_getstate(void)
{
    uint8_t s = dfu_state;
    ep0_tx(&s, 1);
    ctrl_stage = CTRL_STATUS_IN;
}

static void dfu_handle_clrstatus(void)
{
    dfu_state = STATE_DFU_IDLE;
    dfu_status = STATUS_OK;
    ep0_tx(0, 0);
    ctrl_stage = CTRL_IDLE;
}

/* ---- Standard Request Handlers ---- */
static void handle_get_descriptor(void)
{
    uint8_t  dtype = setup_pkt.wValue >> 8;
    uint16_t len = setup_pkt.wLength;
    const uint8_t *desc = 0;
    uint16_t dlen = 0;

    switch (dtype)
    {
        case USB_DESC_DEVICE:
            desc = dev_desc; dlen = sizeof(dev_desc);
            break;
        case USB_DESC_CONFIGURATION:
            desc = cfg_desc; dlen = sizeof(cfg_desc);
            break;
        case USB_DESC_STRING:
            desc = get_string_desc(setup_pkt.wValue & 0xFF, &dlen);
            break;
        default:
            ep0_tx_stall();
            return;
    }

    if (!desc) { ep0_tx_stall(); return; }
    if (dlen > len) dlen = len;

    tx_ptr = desc;
    tx_remaining = dlen;

    uint16_t chunk = (dlen > EP0_MAX_PKT) ? EP0_MAX_PKT : dlen;
    ep0_tx(desc, chunk);
    tx_ptr += chunk;
    tx_remaining -= chunk;
    ctrl_stage = CTRL_DATA_IN;
}

static void handle_set_address(void)
{
    pending_addr = setup_pkt.wValue & 0x7F;
    ep0_tx(0, 0);
    ctrl_stage = CTRL_STATUS_IN;
}

static void handle_set_configuration(void)
{
    ep0_tx(0, 0);
    ctrl_stage = CTRL_STATUS_IN;
}

static void handle_get_status_std(void)
{
    static const uint8_t stat[2] = {0, 0};
    ep0_tx(stat, 2);
    ctrl_stage = CTRL_STATUS_IN;
}

static void handle_get_config(void)
{
    static const uint8_t cfg = 1;
    ep0_tx(&cfg, 1);
    ctrl_stage = CTRL_STATUS_IN;
}

/* ---- SETUP packet dispatch ---- */
static void handle_setup(void)
{
    pma_read(EP0_RX_ADDR, (uint8_t*)&setup_pkt, 8);

    uint8_t req_type = setup_pkt.bmRequestType & 0x60;

    if (req_type == 0x00) /* Standard */
    {
        switch (setup_pkt.bRequest)
        {
            case USB_REQ_GET_DESCRIPTOR:    handle_get_descriptor(); break;
            case USB_REQ_SET_ADDRESS:       handle_set_address(); break;
            case USB_REQ_SET_CONFIGURATION: handle_set_configuration(); break;
            case USB_REQ_GET_STATUS:        handle_get_status_std(); break;
            case USB_REQ_GET_CONFIGURATION: handle_get_config(); break;
            default: ep0_tx_stall(); break;
        }
    }
    else if (req_type == 0x20) /* Class */
    {
        switch (setup_pkt.bRequest)
        {
            case DFU_DNLOAD:    dfu_handle_dnload(); break;
            case DFU_GETSTATUS: dfu_handle_getstatus(); break;
            case DFU_GETSTATE:  dfu_handle_getstate(); break;
            case DFU_CLRSTATUS: dfu_handle_clrstatus(); break;
            case DFU_ABORT:     dfu_handle_clrstatus(); break;
            default: ep0_tx_stall(); break;
        }
    }
    else
    {
        ep0_tx_stall();
    }
}

/* ---- EP0 OUT data handler (multi-packet DFU_DNLOAD) ---- */
static void handle_ep0_rx_data(void)
{
    uint16_t count = PMA(BDT_COUNT_RX) & 0x03FF;
    uint16_t space = DFU_XFER_SIZE - dfu_buf_offset;
    if (count > space) count = space;

    pma_read(EP0_RX_ADDR, dfu_buffer + dfu_buf_offset, count);
    dfu_buf_offset += count;
    ctrl_received += count;

    if (ctrl_received >= ctrl_total_len)
    {
        /* All data received, send ZLP status, prepare for flash */
        dfu_data_len = dfu_buf_offset;
        dfu_state = STATE_DFU_DNLOAD_SYNC;
        ep0_tx(0, 0);
        ctrl_stage = CTRL_IDLE;
    }
    else
    {
        ep_set_rx_status(0, EP_RX_VALID);
    }
}

/* ---- EP0 IN handler (multi-packet descriptor read / status) ---- */
static void handle_ep0_tx_complete(void)
{
    if (pending_addr)
    {
        USB_DADDR = 0x0080U | pending_addr;
        pending_addr = 0;
    }

    if (ctrl_stage == CTRL_DATA_IN && tx_remaining > 0)
    {
        uint16_t chunk = (tx_remaining > EP0_MAX_PKT) ? EP0_MAX_PKT : tx_remaining;
        ep0_tx(tx_ptr, chunk);
        tx_ptr += chunk;
        tx_remaining -= chunk;
    }
    else
    {
        /* Status or transfer done, prepare for next SETUP */
        ctrl_stage = CTRL_IDLE;
        ep_set_rx_status(0, EP_RX_VALID);
    }
}

/* ---- USB Reset handler ---- */
static void usb_reset(void)
{
    USB_BTABLE = 0;

    /* Configure BDT for EP0 */
    PMA(BDT_ADDR_TX) = EP0_TX_ADDR;
    PMA(BDT_COUNT_TX) = 0;
    PMA(BDT_ADDR_RX) = EP0_RX_ADDR;
    /* COUNT_RX: BL_SIZE=1, NUM_BLOCK=1 → 64 bytes */
    PMA(BDT_COUNT_RX) = 0x8400U;

    /* Configure EP0 as Control, TX NAK, RX VALID */
    USB_EPR(0) = EP_TYPE_CONTROL | 0; /* EA=0, toggle bits start at 0 */
    ep_set_tx_status(0, EP_TX_NAK);
    ep_set_rx_status(0, EP_RX_VALID);

    USB_DADDR = 0x0080U; /* Enable USB function, address 0 */

    dfu_state = STATE_DFU_IDLE;
    dfu_status = STATUS_OK;
    ctrl_stage = CTRL_IDLE;
    pending_addr = 0;
    tx_remaining = 0;
}

/* ---- Public API ---- */
void usb_dfu_init(void)
{
    dfu_complete_flag = 0;

    /* Force USB re-enumeration: pull D+ (PA12) low briefly */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    /* PA12 = output push-pull 2MHz (CNF=00, MODE=10 → bits 19:16 = 0b0010) */
    GPIOA->CRH = (GPIOA->CRH & ~(0xFU << 16)) | (0x2U << 16);
    GPIOA->BRR = (1U << 12);
    for (volatile int i = 0; i < 200000; i++);

    /* Release PA12 for USB peripheral */
    GPIOA->CRH = (GPIOA->CRH & ~(0xFU << 16)) | (0x4U << 16); /* input floating */

    /* Enable USB peripheral clock */
    RCC->APB1ENR |= RCC_APB1ENR_USBEN;

    /* Force USB reset */
    USB_CNTR = 0x0003U; /* FRES | PDWN */
    for (volatile int i = 0; i < 100; i++);
    USB_CNTR = 0x0001U; /* Clear PDWN, keep FRES */
    for (volatile int i = 0; i < 100; i++);
    USB_CNTR = 0x0000U; /* Clear FRES */
    USB_ISTR = 0;

    /* Enable RESET and CTR interrupts (polled) */
    USB_CNTR = 0x8400U; /* CTRM | RESETM */
}

void usb_dfu_poll(void)
{
    uint16_t istr = USB_ISTR;

    if (istr & 0x0400U) /* RESET */
    {
        USB_ISTR = ~0x0400U;
        usb_reset();
        return;
    }

    if (istr & 0x8000U) /* CTR */
    {
        uint8_t ep_id = istr & 0x0FU;

        if (ep_id == 0)
        {
            uint16_t epr = USB_EPR(0);

            if (epr & EP_SETUP)
            {
                ep_clear_ctr_rx(0);
                handle_setup();
            }
            else if (epr & EP_CTR_RX)
            {
                ep_clear_ctr_rx(0);
                if (ctrl_stage == CTRL_DATA_OUT)
                    handle_ep0_rx_data();
                else
                    ep_set_rx_status(0, EP_RX_VALID);
            }

            if (epr & EP_CTR_TX)
            {
                ep_clear_ctr_tx(0);
                handle_ep0_tx_complete();
            }
        }
    }
}

int usb_dfu_is_complete(void)
{
    return dfu_complete_flag;
}
