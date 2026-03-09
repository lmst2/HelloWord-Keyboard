#include "usb_dfu.h"
#include "stm32f1xx.h"

/* ---- STM32F103 USB registers ---- */
#define USB_BASE_ADDR               0x40005C00U
#define USB_PMA_BASE_ADDR           0x40006000U

#define USB_EPR(ep)                 (*(volatile uint16_t *)(USB_BASE_ADDR + (uint32_t)(ep) * 4U))
#define USB_CNTR                    (*(volatile uint16_t *)(USB_BASE_ADDR + 0x40U))
#define USB_ISTR                    (*(volatile uint16_t *)(USB_BASE_ADDR + 0x44U))
#define USB_DADDR                   (*(volatile uint16_t *)(USB_BASE_ADDR + 0x4CU))
#define USB_BTABLE                  (*(volatile uint16_t *)(USB_BASE_ADDR + 0x50U))
#define USB_PMA(offset)             (*(volatile uint16_t *)(USB_PMA_BASE_ADDR + ((uint32_t)(offset) << 1)))

#define USB_ISTR_CTR                0x8000U
#define USB_ISTR_RESET              0x0400U

#define EP_CTR_RX                   0x8000U
#define EP_STAT_RX                  0x3000U
#define EP_SETUP                    0x0800U
#define EP_TYPE                     0x0600U
#define EP_KIND                     0x0100U
#define EP_CTR_TX                   0x0080U
#define EP_STAT_TX                  0x0030U
#define EP_EA                       0x000FU

#define EP_TX_STALL                 0x0010U
#define EP_TX_NAK                   0x0020U
#define EP_TX_VALID                 0x0030U
#define EP_RX_VALID                 0x3000U

#define EP_TYPE_CONTROL             0x0200U
#define EP_RW_MASK                  (EP_CTR_RX | EP_TYPE | EP_KIND | EP_CTR_TX | EP_EA)

#define BDT_ADDR_TX                 0U
#define BDT_COUNT_TX                2U
#define BDT_ADDR_RX                 4U
#define BDT_COUNT_RX                6U

#define EP0_TX_ADDR                 0x40U
#define EP0_RX_ADDR                 0x80U
#define EP0_MAX_PACKET              64U

#define USB_REQ_TYPE_STANDARD       0x00U
#define USB_REQ_TYPE_CLASS          0x20U
#define USB_REQ_TYPE_MASK           0x60U

#define DFU_STATUS_DEPTH            6U
#define DFU_CONFIGURATION_SIZE      27U

typedef enum
{
    CTRL_STAGE_IDLE = 0,
    CTRL_STAGE_DATA_OUT,
    CTRL_STAGE_DATA_IN,
    CTRL_STAGE_STATUS_IN
} ctrl_stage_t;

typedef enum
{
    DFU_PENDING_NONE = 0,
    DFU_PENDING_SET_ADDRESS,
    DFU_PENDING_ERASE,
    DFU_PENDING_WRITE,
    DFU_PENDING_LEAVE
} dfu_pending_op_t;

typedef struct __attribute__((packed))
{
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_t;

typedef struct
{
    usb_setup_t setup;
    uint8_t dev_status[DFU_STATUS_DEPTH];
    dfu_state_t dev_state;
    ctrl_stage_t ctrl_stage;
    dfu_pending_op_t pending_op;
    uint32_t data_ptr;
    uint32_t pending_address;
    uint16_t wblock_num;
    uint16_t wlength;
    uint16_t rx_received;
    uint8_t alt_setting;
    uint8_t configured;
    uint8_t pending_usb_addr;
    uint8_t manifest_complete;
    uint8_t complete_flag;
    const uint8_t *tx_ptr;
    uint16_t tx_remaining;
    uint8_t buffer[DFU_XFER_SIZE];
} dfu_context_t;

static dfu_context_t g_dfu;
static uint8_t string_desc_buf[128];

/* ---- Descriptors ---- */
static const uint8_t dev_desc[] = {
    18,
    USB_DESC_DEVICE,
    0x00, 0x02,
    0x00,
    0x00,
    0x00,
    EP0_MAX_PACKET,
    USB_VID & 0xFF, USB_VID >> 8,
    USB_PID & 0xFF, USB_PID >> 8,
    0x01, 0x02,
    1,
    2,
    3,
    1
};

static const uint8_t cfg_desc[] = {
    9, USB_DESC_CONFIGURATION,
    DFU_CONFIGURATION_SIZE, 0x00,
    0x01,
    DFU_CONFIG_VALUE,
    4,
    0x80,
    50,

    9, USB_DESC_INTERFACE,
    0x00,
    DFU_DEFAULT_ALT_SETTING,
    0x00,
    0xFE,
    0x01,
    0x02,
    5,

    9, USB_DESC_DFU_FUNCTIONAL,
    0x0B,
    0xFF, 0x00,
    DFU_XFER_SIZE & 0xFF, DFU_XFER_SIZE >> 8,
    0x1A, 0x01
};

static const uint8_t dfu_func_desc[] = {
    9,
    USB_DESC_DFU_FUNCTIONAL,
    0x0B,
    0xFF, 0x00,
    DFU_XFER_SIZE & 0xFF, DFU_XFER_SIZE >> 8,
    0x1A, 0x01
};

static const uint8_t dev_qual_desc[] = {
    10,
    USB_DESC_DEVICE_QUALIFIER,
    0x00, 0x02,
    0x00,
    0x00,
    0x00,
    EP0_MAX_PACKET,
    0x01,
    0x00
};

static const uint8_t lang_id_desc[] = {
    4,
    USB_DESC_STRING,
    USB_LANGID_EN_US & 0xFF,
    USB_LANGID_EN_US >> 8
};

/* ---- Low level helpers ---- */
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
    USB_EPR(ep) = (val & EP_RW_MASK) & (uint16_t)~EP_CTR_RX;
}

static void ep_clear_ctr_tx(uint8_t ep)
{
    uint16_t val = USB_EPR(ep);
    USB_EPR(ep) = (val & EP_RW_MASK) & (uint16_t)~EP_CTR_TX;
}

static void pma_write(uint16_t pma_offset, const uint8_t *buf, uint16_t len)
{
    for (uint16_t i = 0; i < (uint16_t)((len + 1U) / 2U); ++i)
    {
        uint16_t val = buf[i * 2U];
        if ((uint16_t)(i * 2U + 1U) < len)
            val |= (uint16_t)buf[i * 2U + 1U] << 8;
        USB_PMA(pma_offset) = val;
        pma_offset += 2U;
    }
}

static void pma_read(uint16_t pma_offset, uint8_t *buf, uint16_t len)
{
    for (uint16_t i = 0; i < (uint16_t)((len + 1U) / 2U); ++i)
    {
        uint16_t val = USB_PMA(pma_offset);
        buf[i * 2U] = (uint8_t)(val & 0xFFU);
        if ((uint16_t)(i * 2U + 1U) < len)
            buf[i * 2U + 1U] = (uint8_t)(val >> 8);
        pma_offset += 2U;
    }
}

static void ep0_send_packet(const uint8_t *data, uint16_t len)
{
    if (len > EP0_MAX_PACKET)
        len = EP0_MAX_PACKET;

    pma_write(EP0_TX_ADDR, data, len);
    USB_PMA(BDT_COUNT_TX) = len;
    ep_set_tx_status(0, EP_TX_VALID);
}

static void ep0_send_response(const uint8_t *data, uint16_t len)
{
    uint16_t first_chunk = len;

    if (first_chunk > EP0_MAX_PACKET)
        first_chunk = EP0_MAX_PACKET;

    g_dfu.tx_ptr = data;
    g_dfu.tx_remaining = len;
    if (g_dfu.tx_remaining >= first_chunk)
    {
        g_dfu.tx_ptr += first_chunk;
        g_dfu.tx_remaining -= first_chunk;
    }

    ep0_send_packet(data, first_chunk);
    g_dfu.ctrl_stage = (len > first_chunk) ? CTRL_STAGE_DATA_IN : CTRL_STAGE_STATUS_IN;
}

static void ep0_send_zlp(void)
{
    g_dfu.tx_ptr = 0;
    g_dfu.tx_remaining = 0;
    ep0_send_packet(0, 0);
    g_dfu.ctrl_stage = CTRL_STAGE_STATUS_IN;
}

static void ep0_stall(void)
{
    ep_set_tx_status(0, EP_TX_STALL);
    ep_set_rx_status(0, EP_RX_VALID);
}

/* ---- String descriptors ---- */
static const uint8_t *get_string_desc(uint8_t index, uint16_t *length)
{
    const char *str = 0;
    uint16_t str_len = 0;
    uint16_t total_len = 0;

    if (index == 0U)
    {
        *length = sizeof(lang_id_desc);
        return lang_id_desc;
    }

    switch (index)
    {
        case 1: str = "STMicroelectronics"; break;
        case 2: str = "HelloWord Keyboard DfuSe"; break;
        case 3: str = "HW75-BL-DFUSE"; break;
        case 4: str = "DFU Config"; break;
        case 5: str = "@Internal Flash /0x08000000/16*001Ka,112*001Kg"; break;
        default:
            *length = 0;
            return 0;
    }

    while (str[str_len] != '\0')
        ++str_len;

    total_len = (uint16_t)(2U + str_len * 2U);
    if (total_len > sizeof(string_desc_buf))
        total_len = sizeof(string_desc_buf);

    string_desc_buf[0] = (uint8_t)total_len;
    string_desc_buf[1] = USB_DESC_STRING;
    for (uint16_t i = 0; i < str_len && (uint16_t)(2U + i * 2U + 1U) < total_len; ++i)
    {
        string_desc_buf[2U + i * 2U] = (uint8_t)str[i];
        string_desc_buf[2U + i * 2U + 1U] = 0U;
    }

    *length = total_len;
    return string_desc_buf;
}

/* ---- DFU status helpers ---- */
static void dfu_update_status_fields(void)
{
    g_dfu.dev_status[4] = (uint8_t)g_dfu.dev_state;
    g_dfu.dev_status[5] = 0U;
}

static void dfu_set_poll_timeout(uint16_t timeout_ms)
{
    g_dfu.dev_status[1] = (uint8_t)(timeout_ms & 0xFFU);
    g_dfu.dev_status[2] = (uint8_t)((timeout_ms >> 8) & 0xFFU);
    g_dfu.dev_status[3] = 0U;
}

static void dfu_set_status(dfu_status_t status)
{
    g_dfu.dev_status[0] = (uint8_t)status;
}

static void dfu_reset_status(void)
{
    dfu_set_status(DFU_STATUS_OK);
    dfu_set_poll_timeout(0U);
    dfu_update_status_fields();
}

static void dfu_set_error(dfu_status_t status)
{
    g_dfu.dev_state = DFU_STATE_ERROR;
    g_dfu.pending_op = DFU_PENDING_NONE;
    dfu_set_status(status);
    dfu_set_poll_timeout(0U);
    dfu_update_status_fields();
}

static void dfu_send_status(void)
{
    dfu_update_status_fields();
    ep0_send_response(g_dfu.dev_status, DFU_STATUS_DEPTH);
}

/* ---- Media helpers ---- */
static uint32_t parse_le32(const uint8_t *buf)
{
    return (uint32_t)buf[0]
         | ((uint32_t)buf[1] << 8)
         | ((uint32_t)buf[2] << 16)
         | ((uint32_t)buf[3] << 24);
}

static int dfu_media_can_read(uint32_t addr, uint16_t len)
{
    uint32_t end = addr + (uint32_t)len;

    return (addr >= FLASH_START_ADDRESS) && (end <= FLASH_END_ADDRESS) && (end >= addr);
}

static int dfu_media_can_write(uint32_t addr, uint16_t len)
{
    uint32_t end = addr + (uint32_t)len;

    return (addr >= APP_ADDRESS) && (end <= FLASH_END_ADDRESS) && (end >= addr);
}

static int dfu_media_can_erase(uint32_t addr)
{
    return (addr >= APP_ADDRESS) && (addr < FLASH_END_ADDRESS);
}

static int flash_wait_ready(void)
{
    while ((FLASH->SR & FLASH_SR_BSY) != 0U)
    {}

    if ((FLASH->SR & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) != 0U)
        return -1;
    return 0;
}

static void flash_clear_status_flags(void)
{
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR;
}

static void flash_unlock(void)
{
    if ((FLASH->CR & FLASH_CR_LOCK) != 0U)
    {
        FLASH->KEYR = 0x45670123U;
        FLASH->KEYR = 0xCDEF89ABU;
    }
}

static void flash_lock(void)
{
    FLASH->CR |= FLASH_CR_LOCK;
}

static int flash_erase_page_raw(uint32_t addr)
{
    flash_clear_status_flags();
    if (flash_wait_ready() != 0)
        return -1;

    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR = addr;
    FLASH->CR |= FLASH_CR_STRT;

    if (flash_wait_ready() != 0)
    {
        FLASH->CR &= (uint16_t)~FLASH_CR_PER;
        return -1;
    }

    FLASH->CR &= (uint16_t)~FLASH_CR_PER;
    return 0;
}

static int flash_write_raw(uint32_t addr, const uint8_t *data, uint16_t len)
{
    flash_clear_status_flags();
    if (flash_wait_ready() != 0)
        return -1;

    FLASH->CR |= FLASH_CR_PG;
    for (uint16_t i = 0; i < len; i += 2U)
    {
        uint16_t half_word = data[i];
        if ((uint16_t)(i + 1U) < len)
            half_word |= (uint16_t)data[i + 1U] << 8;
        else
            half_word |= 0xFF00U;

        *(volatile uint16_t *)(addr + i) = half_word;
        if (flash_wait_ready() != 0)
        {
            FLASH->CR &= (uint16_t)~FLASH_CR_PG;
            return -1;
        }
    }
    FLASH->CR &= (uint16_t)~FLASH_CR_PG;
    return 0;
}

static int dfu_media_set_address(uint32_t addr)
{
    if (!dfu_media_can_read(addr, 0U))
        return -1;

    g_dfu.data_ptr = addr;
    return 0;
}

static int dfu_media_erase(uint32_t addr)
{
    int rc;

    if (!dfu_media_can_erase(addr))
        return -1;

    flash_unlock();
    rc = flash_erase_page_raw(addr);
    flash_lock();
    return rc;
}

static int dfu_media_write(uint32_t addr, const uint8_t *data, uint16_t len)
{
    int rc;

    if (!dfu_media_can_write(addr, len))
        return -1;

    flash_unlock();
    rc = flash_write_raw(addr, data, len);
    flash_lock();
    return rc;
}

static const uint8_t *dfu_media_read(uint32_t addr, uint16_t req_len, uint16_t *actual_len)
{
    uint32_t remaining;

    if (addr >= FLASH_END_ADDRESS)
    {
        *actual_len = 0U;
        return 0;
    }

    remaining = FLASH_END_ADDRESS - addr;
    if (req_len > remaining)
        req_len = (uint16_t)remaining;
    if (req_len > DFU_XFER_SIZE)
        req_len = DFU_XFER_SIZE;

    if (!dfu_media_can_read(addr, req_len))
    {
        *actual_len = 0U;
        return 0;
    }

    *actual_len = req_len;
    return (const uint8_t *)addr;
}

static void dfu_media_leave(void)
{
    g_dfu.complete_flag = 1U;
    g_dfu.manifest_complete = 1U;
}

/* ---- Core state helpers ---- */
static void USBD_DFU_ResetState(void)
{
    g_dfu.dev_state = DFU_STATE_IDLE;
    g_dfu.ctrl_stage = CTRL_STAGE_IDLE;
    g_dfu.pending_op = DFU_PENDING_NONE;
    g_dfu.data_ptr = FLASH_START_ADDRESS;
    g_dfu.pending_address = 0U;
    g_dfu.wblock_num = 0U;
    g_dfu.wlength = 0U;
    g_dfu.rx_received = 0U;
    g_dfu.alt_setting = DFU_DEFAULT_ALT_SETTING;
    g_dfu.pending_usb_addr = 0U;
    g_dfu.manifest_complete = 0U;
    g_dfu.complete_flag = 0U;
    g_dfu.tx_ptr = 0;
    g_dfu.tx_remaining = 0U;
    dfu_reset_status();
}

static void DFU_Leave(void)
{
    dfu_media_leave();
    g_dfu.dev_state = DFU_STATE_MANIFEST_WAIT_RESET;
    dfu_set_status(DFU_STATUS_OK);
    dfu_set_poll_timeout(0U);
    dfu_update_status_fields();
    g_dfu.pending_op = DFU_PENDING_NONE;
}

static void DFU_ExecutePending(void)
{
    uint32_t write_addr;
    int rc = -1;

    switch (g_dfu.pending_op)
    {
        case DFU_PENDING_SET_ADDRESS:
            rc = dfu_media_set_address(g_dfu.pending_address);
            if (rc == 0)
            {
                g_dfu.dev_state = DFU_STATE_DNLOAD_IDLE;
                dfu_set_status(DFU_STATUS_OK);
            }
            else
            {
                dfu_set_error(DFU_STATUS_ERR_ADDRESS);
                return;
            }
            break;

        case DFU_PENDING_ERASE:
            rc = dfu_media_erase(g_dfu.pending_address);
            if (rc == 0)
            {
                g_dfu.dev_state = DFU_STATE_DNLOAD_IDLE;
                dfu_set_status(DFU_STATUS_OK);
            }
            else
            {
                dfu_set_error(DFU_STATUS_ERR_ERASE);
                return;
            }
            break;

        case DFU_PENDING_WRITE:
            write_addr = ((uint32_t)(g_dfu.wblock_num - 2U) * DFU_XFER_SIZE) + g_dfu.data_ptr;
            rc = dfu_media_write(write_addr, g_dfu.buffer, g_dfu.wlength);
            if (rc == 0)
            {
                g_dfu.dev_state = DFU_STATE_DNLOAD_IDLE;
                dfu_set_status(DFU_STATUS_OK);
            }
            else
            {
                dfu_set_error(DFU_STATUS_ERR_WRITE);
                return;
            }
            break;

        case DFU_PENDING_LEAVE:
            DFU_Leave();
            return;

        case DFU_PENDING_NONE:
        default:
            break;
    }

    dfu_set_poll_timeout(0U);
    dfu_update_status_fields();
    g_dfu.pending_op = DFU_PENDING_NONE;
}

/* ---- Class request handlers ---- */
static void DFU_Download(void)
{
    if ((g_dfu.dev_state != DFU_STATE_IDLE) && (g_dfu.dev_state != DFU_STATE_DNLOAD_IDLE))
    {
        ep0_stall();
        return;
    }

    g_dfu.wblock_num = g_dfu.setup.wValue;
    g_dfu.wlength = (g_dfu.setup.wLength > DFU_XFER_SIZE) ? DFU_XFER_SIZE : g_dfu.setup.wLength;
    g_dfu.rx_received = 0U;
    g_dfu.pending_op = DFU_PENDING_NONE;
    dfu_set_status(DFU_STATUS_OK);

    if (g_dfu.wlength == 0U)
    {
        g_dfu.pending_op = DFU_PENDING_LEAVE;
        g_dfu.dev_state = DFU_STATE_MANIFEST_SYNC;
        dfu_set_poll_timeout(0U);
        dfu_update_status_fields();
        ep0_send_zlp();
        return;
    }

    g_dfu.ctrl_stage = CTRL_STAGE_DATA_OUT;
    ep_set_rx_status(0, EP_RX_VALID);
}

static void DFU_Upload(void)
{
    static const uint8_t commands[] = {
        DFUSE_CMD_GET_COMMANDS,
        DFUSE_CMD_SET_ADDRESS,
        DFUSE_CMD_ERASE
    };
    const uint8_t *data = 0;
    uint16_t actual_len = 0U;
    uint32_t addr;

    if ((g_dfu.dev_state != DFU_STATE_IDLE) && (g_dfu.dev_state != DFU_STATE_UPLOAD_IDLE))
    {
        ep0_stall();
        return;
    }

    g_dfu.wblock_num = g_dfu.setup.wValue;
    g_dfu.wlength = (g_dfu.setup.wLength > DFU_XFER_SIZE) ? DFU_XFER_SIZE : g_dfu.setup.wLength;
    dfu_set_status(DFU_STATUS_OK);
    dfu_set_poll_timeout(0U);

    if (g_dfu.wblock_num == 0U)
    {
        actual_len = sizeof(commands);
        if (actual_len > g_dfu.wlength)
            actual_len = g_dfu.wlength;

        g_dfu.dev_state = (g_dfu.wlength > sizeof(commands)) ? DFU_STATE_IDLE : DFU_STATE_UPLOAD_IDLE;
        dfu_update_status_fields();
        ep0_send_response(commands, actual_len);
        return;
    }

    if (g_dfu.wblock_num <= 1U)
    {
        dfu_set_error(DFU_STATUS_ERR_STALLEDPKT);
        ep0_stall();
        return;
    }

    addr = ((uint32_t)(g_dfu.wblock_num - 2U) * DFU_XFER_SIZE) + g_dfu.data_ptr;
    data = dfu_media_read(addr, g_dfu.wlength, &actual_len);
    if (data == 0)
    {
        dfu_set_error(DFU_STATUS_ERR_STALLEDPKT);
        ep0_stall();
        return;
    }

    g_dfu.dev_state = (actual_len < g_dfu.wlength) ? DFU_STATE_IDLE : DFU_STATE_UPLOAD_IDLE;
    dfu_update_status_fields();
    ep0_send_response(data, actual_len);
}

static void DFU_GetStatus(void)
{
    switch (g_dfu.dev_state)
    {
        case DFU_STATE_DNLOAD_SYNC:
            g_dfu.dev_state = DFU_STATE_DNLOAD_BUSY;
            if (g_dfu.pending_op == DFU_PENDING_ERASE)
                dfu_set_poll_timeout(50U);
            else if (g_dfu.pending_op == DFU_PENDING_WRITE)
                dfu_set_poll_timeout(10U);
            else
                dfu_set_poll_timeout(1U);
            break;

        case DFU_STATE_MANIFEST_SYNC:
            g_dfu.dev_state = DFU_STATE_MANIFEST;
            dfu_set_poll_timeout(1U);
            break;

        default:
            break;
    }

    dfu_update_status_fields();
    dfu_send_status();
}

static void DFU_ClearStatus(void)
{
    if (g_dfu.dev_state == DFU_STATE_ERROR)
    {
        g_dfu.dev_state = DFU_STATE_IDLE;
        g_dfu.pending_op = DFU_PENDING_NONE;
        dfu_reset_status();
    }
    else
    {
        dfu_set_error(DFU_STATUS_ERR_UNKNOWN);
    }
    ep0_send_zlp();
}

static void DFU_GetState(void)
{
    uint8_t state = (uint8_t)g_dfu.dev_state;
    ep0_send_response(&state, 1U);
}

static void DFU_Abort(void)
{
    g_dfu.dev_state = DFU_STATE_IDLE;
    g_dfu.pending_op = DFU_PENDING_NONE;
    g_dfu.wlength = 0U;
    g_dfu.wblock_num = 0U;
    g_dfu.rx_received = 0U;
    dfu_reset_status();
    ep0_send_zlp();
}

static void DFU_Detach(void)
{
    ep0_send_zlp();
}

/* ---- Standard request handlers ---- */
static void handle_get_descriptor(void)
{
    const uint8_t *desc = 0;
    uint16_t desc_len = 0U;
    uint8_t dtype = (uint8_t)(g_dfu.setup.wValue >> 8);

    switch (dtype)
    {
        case USB_DESC_DEVICE:
            desc = dev_desc;
            desc_len = sizeof(dev_desc);
            break;

        case USB_DESC_CONFIGURATION:
            desc = cfg_desc;
            desc_len = sizeof(cfg_desc);
            break;

        case USB_DESC_STRING:
            desc = get_string_desc((uint8_t)(g_dfu.setup.wValue & 0xFFU), &desc_len);
            break;

        case USB_DESC_DEVICE_QUALIFIER:
            desc = dev_qual_desc;
            desc_len = sizeof(dev_qual_desc);
            break;

        case USB_DESC_DFU_FUNCTIONAL:
            desc = dfu_func_desc;
            desc_len = sizeof(dfu_func_desc);
            break;

        default:
            ep0_stall();
            return;
    }

    if (desc == 0)
    {
        ep0_stall();
        return;
    }

    if (desc_len > g_dfu.setup.wLength)
        desc_len = g_dfu.setup.wLength;

    ep0_send_response(desc, desc_len);
}

static void handle_set_address(void)
{
    g_dfu.pending_usb_addr = (uint8_t)(g_dfu.setup.wValue & 0x7FU);
    ep0_send_zlp();
}

static void handle_get_status_std(void)
{
    static const uint8_t status_info[2] = {0, 0};

    if (!g_dfu.configured)
    {
        ep0_stall();
        return;
    }

    ep0_send_response(status_info, sizeof(status_info));
}

static void handle_get_configuration(void)
{
    uint8_t cfg = g_dfu.configured ? DFU_CONFIG_VALUE : 0U;
    ep0_send_response(&cfg, 1U);
}

static void handle_set_configuration(void)
{
    g_dfu.configured = (g_dfu.setup.wValue == DFU_CONFIG_VALUE) ? 1U : 0U;
    ep0_send_zlp();
}

static void handle_get_interface(void)
{
    if (!g_dfu.configured)
    {
        ep0_stall();
        return;
    }

    ep0_send_response(&g_dfu.alt_setting, 1U);
}

static void handle_set_interface(void)
{
    if (!g_dfu.configured)
    {
        ep0_stall();
        return;
    }

    if (g_dfu.setup.wValue >= DFU_ALT_INTERFACE_COUNT)
    {
        ep0_stall();
        return;
    }

    g_dfu.alt_setting = (uint8_t)g_dfu.setup.wValue;
    ep0_send_zlp();
}

/* ---- Setup dispatcher ---- */
static void USBD_DFU_Setup(void)
{
    switch (g_dfu.setup.bmRequestType & USB_REQ_TYPE_MASK)
    {
        case USB_REQ_TYPE_CLASS:
            switch (g_dfu.setup.bRequest)
            {
                case DFU_DNLOAD:    DFU_Download(); break;
                case DFU_UPLOAD:    DFU_Upload(); break;
                case DFU_GETSTATUS: DFU_GetStatus(); break;
                case DFU_CLRSTATUS: DFU_ClearStatus(); break;
                case DFU_GETSTATE:  DFU_GetState(); break;
                case DFU_ABORT:     DFU_Abort(); break;
                case DFU_DETACH:    DFU_Detach(); break;
                default:            ep0_stall(); break;
            }
            break;

        case USB_REQ_TYPE_STANDARD:
            switch (g_dfu.setup.bRequest)
            {
                case USB_REQ_GET_STATUS:        handle_get_status_std(); break;
                case USB_REQ_GET_DESCRIPTOR:    handle_get_descriptor(); break;
                case USB_REQ_GET_INTERFACE:     handle_get_interface(); break;
                case USB_REQ_SET_INTERFACE:     handle_set_interface(); break;
                case USB_REQ_GET_CONFIGURATION: handle_get_configuration(); break;
                case USB_REQ_SET_CONFIGURATION: handle_set_configuration(); break;
                case USB_REQ_SET_ADDRESS:       handle_set_address(); break;
                case USB_REQ_CLEAR_FEATURE:     ep0_send_zlp(); break;
                default:                        ep0_stall(); break;
            }
            break;

        default:
            ep0_stall();
            break;
    }
}

/* ---- EP0 stage handlers ---- */
static void prepare_pending_download_op(void)
{
    if (g_dfu.wblock_num == 0U)
    {
        if (g_dfu.wlength == 5U && g_dfu.buffer[0] == DFUSE_CMD_SET_ADDRESS)
        {
            g_dfu.pending_address = parse_le32(&g_dfu.buffer[1]);
            g_dfu.pending_op = DFU_PENDING_SET_ADDRESS;
            g_dfu.dev_state = DFU_STATE_DNLOAD_SYNC;
            return;
        }

        if (g_dfu.wlength == 5U && g_dfu.buffer[0] == DFUSE_CMD_ERASE)
        {
            g_dfu.pending_address = parse_le32(&g_dfu.buffer[1]);
            g_dfu.pending_op = DFU_PENDING_ERASE;
            g_dfu.dev_state = DFU_STATE_DNLOAD_SYNC;
            return;
        }

        dfu_set_error(DFU_STATUS_ERR_STALLEDPKT);
        return;
    }

    if (g_dfu.wblock_num >= 2U)
    {
        g_dfu.pending_op = DFU_PENDING_WRITE;
        g_dfu.dev_state = DFU_STATE_DNLOAD_SYNC;
        return;
    }

    dfu_set_error(DFU_STATUS_ERR_STALLEDPKT);
}

static void USBD_DFU_EP0_RxReady(void)
{
    uint16_t count = USB_PMA(BDT_COUNT_RX) & 0x03FFU;
    uint16_t remaining = DFU_XFER_SIZE - g_dfu.rx_received;

    if (count > remaining)
        count = remaining;

    pma_read(EP0_RX_ADDR, &g_dfu.buffer[g_dfu.rx_received], count);
    g_dfu.rx_received += count;

    if (g_dfu.rx_received < g_dfu.wlength)
    {
        ep_set_rx_status(0, EP_RX_VALID);
        return;
    }

    prepare_pending_download_op();
    ep0_send_zlp();
}

static void USBD_DFU_EP0_TxReady(void)
{
    if (g_dfu.pending_usb_addr != 0U)
    {
        USB_DADDR = 0x0080U | g_dfu.pending_usb_addr;
        g_dfu.pending_usb_addr = 0U;
    }

    if (g_dfu.ctrl_stage == CTRL_STAGE_DATA_IN && g_dfu.tx_remaining > 0U)
    {
        uint16_t chunk = g_dfu.tx_remaining;

        if (chunk > EP0_MAX_PACKET)
            chunk = EP0_MAX_PACKET;

        ep0_send_packet(g_dfu.tx_ptr, chunk);
        g_dfu.tx_ptr += chunk;
        g_dfu.tx_remaining -= chunk;
        return;
    }

    if (g_dfu.dev_state == DFU_STATE_DNLOAD_BUSY)
    {
        DFU_ExecutePending();
    }
    else if (g_dfu.dev_state == DFU_STATE_MANIFEST)
    {
        DFU_Leave();
    }

    g_dfu.ctrl_stage = CTRL_STAGE_IDLE;
    ep_set_rx_status(0, EP_RX_VALID);
}

/* ---- USB reset / public API ---- */
static void usb_reset(void)
{
    USB_BTABLE = 0U;
    USB_PMA(BDT_ADDR_TX) = EP0_TX_ADDR;
    USB_PMA(BDT_COUNT_TX) = 0U;
    USB_PMA(BDT_ADDR_RX) = EP0_RX_ADDR;
    USB_PMA(BDT_COUNT_RX) = 0x8400U;

    USB_EPR(0) = EP_TYPE_CONTROL | 0U;
    ep_set_tx_status(0, EP_TX_NAK);
    ep_set_rx_status(0, EP_RX_VALID);
    USB_DADDR = 0x0080U;

    g_dfu.configured = 0U;
    USBD_DFU_ResetState();
}

void usb_dfu_init(void)
{
    /* Force host re-enumeration by pulling D+ low briefly. */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    GPIOA->CRH = (GPIOA->CRH & ~(0xFU << 16)) | (0x2U << 16);
    GPIOA->BRR = (1U << 12);
    for (volatile int i = 0; i < 200000; ++i)
    {}

    GPIOA->CRH = (GPIOA->CRH & ~(0xFU << 16)) | (0x4U << 16);

    RCC->APB1ENR |= RCC_APB1ENR_USBEN;
    USB_CNTR = 0x0003U;
    for (volatile int i = 0; i < 100; ++i)
    {}
    USB_CNTR = 0x0001U;
    for (volatile int i = 0; i < 100; ++i)
    {}
    USB_CNTR = 0x0000U;
    USB_ISTR = 0U;

    USBD_DFU_ResetState();
    USB_CNTR = 0x8400U;
}

void usb_dfu_poll(void)
{
    uint16_t istr = USB_ISTR;

    if ((istr & USB_ISTR_RESET) != 0U)
    {
        USB_ISTR = (uint16_t)~USB_ISTR_RESET;
        usb_reset();
        return;
    }

    if ((istr & USB_ISTR_CTR) == 0U)
        return;

    if ((istr & 0x0FU) != 0U)
        return;

    {
        uint16_t epr = USB_EPR(0);

        if ((epr & EP_SETUP) != 0U)
        {
            ep_clear_ctr_rx(0);
            pma_read(EP0_RX_ADDR, (uint8_t *)&g_dfu.setup, sizeof(g_dfu.setup));
            USBD_DFU_Setup();
        }
        else if ((epr & EP_CTR_RX) != 0U)
        {
            ep_clear_ctr_rx(0);
            if (g_dfu.ctrl_stage == CTRL_STAGE_DATA_OUT)
                USBD_DFU_EP0_RxReady();
            else
                ep_set_rx_status(0, EP_RX_VALID);
        }

        if ((epr & EP_CTR_TX) != 0U)
        {
            ep_clear_ctr_tx(0);
            USBD_DFU_EP0_TxReady();
        }
    }
}

int usb_dfu_is_complete(void)
{
    return g_dfu.complete_flag;
}
#include "usb_dfu.h"
#include "stm32f1xx.h"

/* ---- STM32F103 USB Peripheral Registers ---- */
#define USB_BASE_ADDR           0x40005C00U
#define USB_PMA_BASE_ADDR       0x40006000U

#define USB_EPR(ep)             (*(volatile uint16_t *)(USB_BASE_ADDR + (uint32_t)(ep) * 4U))
#define USB_CNTR                (*(volatile uint16_t *)(USB_BASE_ADDR + 0x40U))
#define USB_ISTR                (*(volatile uint16_t *)(USB_BASE_ADDR + 0x44U))
#define USB_DADDR               (*(volatile uint16_t *)(USB_BASE_ADDR + 0x4CU))
#define USB_BTABLE              (*(volatile uint16_t *)(USB_BASE_ADDR + 0x50U))
#define USB_PMA(offset)         (*(volatile uint16_t *)(USB_PMA_BASE_ADDR + ((uint32_t)(offset) << 1)))

#define USB_ISTR_CTR            0x8000U
#define USB_ISTR_RESET          0x0400U

#define EP_CTR_RX               0x8000U
#define EP_DTOG_RX              0x4000U
#define EP_STAT_RX              0x3000U
#define EP_SETUP                0x0800U
#define EP_TYPE                 0x0600U
#define EP_KIND                 0x0100U
#define EP_CTR_TX               0x0080U
#define EP_DTOG_TX              0x0040U
#define EP_STAT_TX              0x0030U
#define EP_EA                   0x000FU

#define EP_TX_STALL             0x0010U
#define EP_TX_NAK               0x0020U
#define EP_TX_VALID             0x0030U
#define EP_RX_VALID             0x3000U

#define EP_TYPE_CONTROL         0x0200U
#define EP_RW_MASK              (EP_CTR_RX | EP_TYPE | EP_KIND | EP_CTR_TX | EP_EA)

#define BDT_ADDR_TX             0U
#define BDT_COUNT_TX            2U
#define BDT_ADDR_RX             4U
#define BDT_COUNT_RX            6U

#define EP0_TX_ADDR             0x40U
#define EP0_RX_ADDR             0x80U
#define EP0_MAX_PACKET          64U
#define DFU_CONFIGURATION_SIZE  27U

typedef enum
{
    CTRL_STAGE_IDLE = 0,
    CTRL_STAGE_DATA_OUT,
    CTRL_STAGE_DATA_IN,
    CTRL_STAGE_STATUS_IN
} ctrl_stage_t;

typedef enum
{
    DFU_PENDING_NONE = 0,
    DFU_PENDING_SET_ADDRESS,
    DFU_PENDING_ERASE,
    DFU_PENDING_WRITE,
    DFU_PENDING_LEAVE
} dfu_pending_op_t;

typedef struct __attribute__((packed))
{
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_t;

typedef struct
{
    usb_setup_t setup;
    dfu_state_t state;
    dfu_status_t status;
    dfu_pending_op_t pending_op;
    ctrl_stage_t ctrl_stage;
    uint32_t addr_ptr;
    uint32_t pending_addr;
    uint16_t block_num;
    uint16_t data_len;
    uint16_t ctrl_total_len;
    uint16_t ctrl_received;
    uint16_t poll_timeout_ms;
    uint8_t alt_setting;
    uint8_t configured;
    uint8_t pending_usb_addr;
    uint8_t complete_flag;
    const uint8_t *tx_ptr;
    uint16_t tx_remaining;
    uint8_t rx_buffer[DFU_XFER_SIZE];
} dfu_context_t;

static dfu_context_t g_dfu;

/* ---- USB Descriptors ---- */
static const uint8_t dev_desc[] = {
    18,
    USB_DESC_DEVICE,
    0x00, 0x02,
    0x00,
    0x00,
    0x00,
    EP0_MAX_PACKET,
    USB_VID & 0xFF, USB_VID >> 8,
    USB_PID & 0xFF, USB_PID >> 8,
    0x01, 0x02,
    1,
    2,
    3,
    1
};

static const uint8_t cfg_desc[] = {
    9, USB_DESC_CONFIGURATION,
    DFU_CONFIGURATION_SIZE, 0x00,
    0x01,
    DFU_CONFIG_VALUE,
    4,
    0x80,
    50,

    9, USB_DESC_INTERFACE,
    0x00,
    DFU_ALT_APPLICATION,
    0x00,
    0xFE,
    0x01,
    0x02,
    5,

    9, USB_DESC_DFU_FUNCTIONAL,
    0x0B,
    0xFF, 0x00,
    DFU_XFER_SIZE & 0xFF, DFU_XFER_SIZE >> 8,
    0x1A, 0x01
};

static const uint8_t dev_qual_desc[] = {
    10,
    USB_DESC_DEVICE_QUALIFIER,
    0x00, 0x02,
    0x00,
    0x00,
    0x00,
    EP0_MAX_PACKET,
    0x01,
    0x00
};

static const uint8_t dfu_func_desc[] = {
    9,
    USB_DESC_DFU_FUNCTIONAL,
    0x0B,
    0xFF, 0x00,
    DFU_XFER_SIZE & 0xFF, DFU_XFER_SIZE >> 8,
    0x1A, 0x01
};

static const uint8_t lang_id_desc[] = {
    4,
    USB_DESC_STRING,
    USB_LANGID_EN_US & 0xFF,
    USB_LANGID_EN_US >> 8
};

static uint8_t string_desc_buf[128];

/* ---- USB register helpers ---- */
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
    USB_EPR(ep) = (val & EP_RW_MASK) & (uint16_t)~EP_CTR_RX;
}

static void ep_clear_ctr_tx(uint8_t ep)
{
    uint16_t val = USB_EPR(ep);
    USB_EPR(ep) = (val & EP_RW_MASK) & (uint16_t)~EP_CTR_TX;
}

static void pma_write(uint16_t pma_offset, const uint8_t *buf, uint16_t len)
{
    for (uint16_t i = 0; i < (uint16_t)((len + 1U) / 2U); ++i)
    {
        uint16_t val = buf[i * 2U];
        if ((uint16_t)(i * 2U + 1U) < len)
            val |= (uint16_t)buf[i * 2U + 1U] << 8;
        USB_PMA(pma_offset) = val;
        pma_offset += 2U;
    }
}

static void pma_read(uint16_t pma_offset, uint8_t *buf, uint16_t len)
{
    for (uint16_t i = 0; i < (uint16_t)((len + 1U) / 2U); ++i)
    {
        uint16_t val = USB_PMA(pma_offset);
        buf[i * 2U] = (uint8_t)(val & 0xFFU);
        if ((uint16_t)(i * 2U + 1U) < len)
            buf[i * 2U + 1U] = (uint8_t)(val >> 8);
        pma_offset += 2U;
    }
}

static void ep0_send_packet(const uint8_t *data, uint16_t len)
{
    if (len > EP0_MAX_PACKET)
        len = EP0_MAX_PACKET;
    pma_write(EP0_TX_ADDR, data, len);
    USB_PMA(BDT_COUNT_TX) = len;
    ep_set_tx_status(0, EP_TX_VALID);
}

static void ep0_send_response(const uint8_t *data, uint16_t len)
{
    uint16_t first_chunk = len;

    if (first_chunk > EP0_MAX_PACKET)
        first_chunk = EP0_MAX_PACKET;

    g_dfu.tx_ptr = data;
    g_dfu.tx_remaining = len;
    if (g_dfu.tx_remaining >= first_chunk)
    {
        g_dfu.tx_ptr += first_chunk;
        g_dfu.tx_remaining -= first_chunk;
    }

    ep0_send_packet(data, first_chunk);
    g_dfu.ctrl_stage = (len > first_chunk) ? CTRL_STAGE_DATA_IN : CTRL_STAGE_STATUS_IN;
}

static void ep0_send_zlp(void)
{
    g_dfu.tx_ptr = 0;
    g_dfu.tx_remaining = 0;
    ep0_send_packet(0, 0);
    g_dfu.ctrl_stage = CTRL_STAGE_STATUS_IN;
}

static void ep0_stall(void)
{
    ep_set_tx_status(0, EP_TX_STALL);
    ep_set_rx_status(0, EP_RX_VALID);
}

/* ---- String descriptors ---- */
static const uint8_t *get_string_desc(uint8_t index, uint16_t *length)
{
    const char *str = 0;
    uint16_t str_len = 0;
    uint16_t total_len = 0;

    if (index == 0U)
    {
        *length = sizeof(lang_id_desc);
        return lang_id_desc;
    }

    switch (index)
    {
        case 1: str = "STMicroelectronics"; break;
        case 2: str = "HelloWord Keyboard DfuSe"; break;
        case 3: str = "HW75-BL-DFUSE"; break;
        case 4: str = "DFU Config"; break;
        case 5: str = "@Internal Flash /0x08000000/16*001Ka,112*001Kg"; break;
        default:
            *length = 0;
            return 0;
    }

    while (str[str_len] != '\0')
        ++str_len;

    total_len = (uint16_t)(2U + str_len * 2U);
    if (total_len > sizeof(string_desc_buf))
        total_len = sizeof(string_desc_buf);

    string_desc_buf[0] = (uint8_t)total_len;
    string_desc_buf[1] = USB_DESC_STRING;
    for (uint16_t i = 0; i < str_len && (uint16_t)(2U + i * 2U + 1U) < total_len; ++i)
    {
        string_desc_buf[2U + i * 2U] = (uint8_t)str[i];
        string_desc_buf[2U + i * 2U + 1U] = 0;
    }

    *length = total_len;
    return string_desc_buf;
}

/* ---- DfuSe helpers ---- */
static uint32_t parse_le32(const uint8_t *buf)
{
    return (uint32_t)buf[0]
         | ((uint32_t)buf[1] << 8)
         | ((uint32_t)buf[2] << 16)
         | ((uint32_t)buf[3] << 24);
}

static uint32_t dfu_target_start(uint8_t alt_setting)
{
    return (alt_setting == DFU_ALT_BOOTLOADER) ? FLASH_START_ADDRESS : APP_ADDRESS;
}

static uint32_t dfu_target_end(uint8_t alt_setting)
{
    return (alt_setting == DFU_ALT_BOOTLOADER) ? APP_ADDRESS : FLASH_END_ADDRESS;
}

static void dfu_select_alt(uint8_t alt_setting)
{
    g_dfu.alt_setting = alt_setting;
    g_dfu.addr_ptr = dfu_target_start(alt_setting);
}

static void dfu_set_error(dfu_status_t status)
{
    g_dfu.status = status;
    g_dfu.state = DFU_STATE_ERROR;
    g_dfu.pending_op = DFU_PENDING_NONE;
}

static void dfu_set_poll_timeout(uint16_t poll_timeout_ms)
{
    g_dfu.poll_timeout_ms = poll_timeout_ms;
}

static void dfu_send_status_response(void)
{
    uint8_t resp[6];

    resp[0] = (uint8_t)g_dfu.status;
    resp[1] = (uint8_t)(g_dfu.poll_timeout_ms & 0xFFU);
    resp[2] = (uint8_t)((g_dfu.poll_timeout_ms >> 8) & 0xFFU);
    resp[3] = 0U;
    resp[4] = (uint8_t)g_dfu.state;
    resp[5] = 0U;
    ep0_send_response(resp, sizeof(resp));
}

static int dfu_media_can_read(uint32_t addr, uint16_t len)
{
    uint32_t end = addr + (uint32_t)len;

    return (addr >= FLASH_START_ADDRESS) && (end <= FLASH_END_ADDRESS) && (end >= addr);
}

static int dfu_media_can_write(uint32_t addr, uint16_t len)
{
    uint32_t end = addr + (uint32_t)len;

    if (g_dfu.alt_setting != DFU_ALT_APPLICATION)
        return 0;

    return (addr >= APP_ADDRESS) && (end <= FLASH_END_ADDRESS) && (end >= addr);
}

static int dfu_media_can_erase(uint32_t addr)
{
    if (g_dfu.alt_setting != DFU_ALT_APPLICATION)
        return 0;

    return (addr >= APP_ADDRESS) && (addr < FLASH_END_ADDRESS);
}

static int flash_wait_ready(void)
{
    while ((FLASH->SR & FLASH_SR_BSY) != 0U)
    {}

    if ((FLASH->SR & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) != 0U)
        return -1;
    return 0;
}

static void flash_clear_status_flags(void)
{
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR;
}

static void flash_unlock(void)
{
    if ((FLASH->CR & FLASH_CR_LOCK) != 0U)
    {
        FLASH->KEYR = 0x45670123U;
        FLASH->KEYR = 0xCDEF89ABU;
    }
}

static void flash_lock(void)
{
    FLASH->CR |= FLASH_CR_LOCK;
}

static int flash_erase_page_raw(uint32_t addr)
{
    flash_clear_status_flags();
    if (flash_wait_ready() != 0)
        return -1;

    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR = addr;
    FLASH->CR |= FLASH_CR_STRT;

    if (flash_wait_ready() != 0)
    {
        FLASH->CR &= (uint16_t)~FLASH_CR_PER;
        return -1;
    }

    FLASH->CR &= (uint16_t)~FLASH_CR_PER;
    return 0;
}

static int flash_write_raw(uint32_t addr, const uint8_t *data, uint16_t len)
{
    flash_clear_status_flags();
    if (flash_wait_ready() != 0)
        return -1;

    FLASH->CR |= FLASH_CR_PG;
    for (uint16_t i = 0; i < len; i += 2U)
    {
        uint16_t half_word = data[i];
        if ((uint16_t)(i + 1U) < len)
            half_word |= (uint16_t)data[i + 1U] << 8;
        else
            half_word |= 0xFF00U;

        *(volatile uint16_t *)(addr + i) = half_word;
        if (flash_wait_ready() != 0)
        {
            FLASH->CR &= (uint16_t)~FLASH_CR_PG;
            return -1;
        }
    }
    FLASH->CR &= (uint16_t)~FLASH_CR_PG;

    return 0;
}

static int dfu_media_set_address(uint32_t addr)
{
    if (!dfu_media_can_read(addr, 0))
        return -1;

    g_dfu.addr_ptr = addr;
    return 0;
}

static int dfu_media_erase(uint32_t addr)
{
    int rc;

    if (!dfu_media_can_erase(addr))
        return -1;

    flash_unlock();
    rc = flash_erase_page_raw(addr);
    flash_lock();
    return rc;
}

static int dfu_media_write(uint32_t addr, const uint8_t *data, uint16_t len)
{
    int rc;

    if (!dfu_media_can_write(addr, len))
        return -1;

    flash_unlock();
    rc = flash_write_raw(addr, data, len);
    flash_lock();
    return rc;
}

static const uint8_t *dfu_media_read(uint32_t addr, uint16_t req_len, uint16_t *actual_len)
{
    uint32_t target_end = dfu_target_end(g_dfu.alt_setting);
    uint32_t remaining;

    if (addr >= target_end)
    {
        *actual_len = 0;
        return 0;
    }

    remaining = target_end - addr;
    if (req_len > remaining)
        req_len = (uint16_t)remaining;
    if (req_len > DFU_XFER_SIZE)
        req_len = DFU_XFER_SIZE;

    if (!dfu_media_can_read(addr, req_len))
    {
        *actual_len = 0;
        return 0;
    }

    *actual_len = req_len;
    return (const uint8_t *)addr;
}

static void dfu_media_leave(void)
{
    g_dfu.complete_flag = 1;
}

static void dfu_reset_runtime_state(void)
{
    g_dfu.state = DFU_STATE_IDLE;
    g_dfu.status = DFU_STATUS_OK;
    g_dfu.pending_op = DFU_PENDING_NONE;
    g_dfu.ctrl_stage = CTRL_STAGE_IDLE;
    g_dfu.block_num = 0;
    g_dfu.data_len = 0;
    g_dfu.ctrl_total_len = 0;
    g_dfu.ctrl_received = 0;
    g_dfu.poll_timeout_ms = 0;
    g_dfu.pending_addr = 0;
    g_dfu.pending_usb_addr = 0;
    g_dfu.complete_flag = 0;
    g_dfu.tx_ptr = 0;
    g_dfu.tx_remaining = 0;
    g_dfu.alt_setting = DFU_ALT_APPLICATION;
    g_dfu.addr_ptr = FLASH_START_ADDRESS;
}

/* ---- DFU request handling ---- */
static void dfu_begin_dnload(void)
{
    g_dfu.block_num = g_dfu.setup.wValue;
    g_dfu.ctrl_total_len = g_dfu.setup.wLength;
    g_dfu.ctrl_received = 0;
    g_dfu.data_len = 0;
    g_dfu.pending_op = DFU_PENDING_NONE;
    g_dfu.status = DFU_STATUS_OK;

    if (g_dfu.ctrl_total_len == 0U)
    {
        g_dfu.pending_op = DFU_PENDING_LEAVE;
        g_dfu.state = DFU_STATE_MANIFEST_SYNC;
        ep0_send_zlp();
        return;
    }

    if (g_dfu.ctrl_total_len > DFU_XFER_SIZE)
    {
        dfu_set_error(DFU_STATUS_ERR_ADDRESS);
        ep0_stall();
        return;
    }

    g_dfu.ctrl_stage = CTRL_STAGE_DATA_OUT;
    ep_set_rx_status(0, EP_RX_VALID);
}

static void dfu_prepare_pending_op(void)
{
    if (g_dfu.block_num == 0U)
    {
        if (g_dfu.data_len == 5U && g_dfu.rx_buffer[0] == DFUSE_CMD_SET_ADDRESS)
        {
            g_dfu.pending_addr = parse_le32(&g_dfu.rx_buffer[1]);
            g_dfu.pending_op = DFU_PENDING_SET_ADDRESS;
            g_dfu.state = DFU_STATE_DNLOAD_SYNC;
            return;
        }

        if (g_dfu.data_len == 5U && g_dfu.rx_buffer[0] == DFUSE_CMD_ERASE)
        {
            g_dfu.pending_addr = parse_le32(&g_dfu.rx_buffer[1]);
            g_dfu.pending_op = DFU_PENDING_ERASE;
            g_dfu.state = DFU_STATE_DNLOAD_SYNC;
            return;
        }

        dfu_set_error(DFU_STATUS_ERR_STALLEDPKT);
        return;
    }

    if (g_dfu.block_num >= 2U)
    {
        g_dfu.pending_op = DFU_PENDING_WRITE;
        g_dfu.state = DFU_STATE_DNLOAD_SYNC;
        return;
    }

    dfu_set_error(DFU_STATUS_ERR_STALLEDPKT);
}

static void dfu_execute_pending_op(void)
{
    uint32_t write_addr;
    int rc = -1;

    switch (g_dfu.pending_op)
    {
        case DFU_PENDING_SET_ADDRESS:
            rc = dfu_media_set_address(g_dfu.pending_addr);
            if (rc == 0)
            {
                g_dfu.state = DFU_STATE_DNLOAD_IDLE;
                g_dfu.status = DFU_STATUS_OK;
            }
            else
            {
                dfu_set_error(DFU_STATUS_ERR_ADDRESS);
            }
            break;

        case DFU_PENDING_ERASE:
            rc = dfu_media_erase(g_dfu.pending_addr);
            if (rc == 0)
            {
                g_dfu.state = DFU_STATE_DNLOAD_IDLE;
                g_dfu.status = DFU_STATUS_OK;
            }
            else
            {
                dfu_set_error(DFU_STATUS_ERR_ERASE);
            }
            break;

        case DFU_PENDING_WRITE:
            write_addr = g_dfu.addr_ptr + (uint32_t)(g_dfu.block_num - 2U) * DFU_XFER_SIZE;
            rc = dfu_media_write(write_addr, g_dfu.rx_buffer, g_dfu.data_len);
            if (rc == 0)
            {
                g_dfu.state = DFU_STATE_DNLOAD_IDLE;
                g_dfu.status = DFU_STATUS_OK;
            }
            else
            {
                dfu_set_error(DFU_STATUS_ERR_WRITE);
            }
            break;

        case DFU_PENDING_LEAVE:
            dfu_media_leave();
            g_dfu.state = DFU_STATE_MANIFEST_WAIT_RESET;
            g_dfu.status = DFU_STATUS_OK;
            break;

        case DFU_PENDING_NONE:
        default:
            g_dfu.state = DFU_STATE_IDLE;
            g_dfu.status = DFU_STATUS_OK;
            break;
    }

    if (g_dfu.pending_op != DFU_PENDING_LEAVE)
        g_dfu.pending_op = DFU_PENDING_NONE;
}

static void dfu_handle_getstatus(void)
{
    dfu_set_poll_timeout(0U);

    if (g_dfu.state == DFU_STATE_DNLOAD_SYNC)
    {
        g_dfu.state = DFU_STATE_DNLOAD_BUSY;
        if (g_dfu.pending_op == DFU_PENDING_ERASE)
            dfu_set_poll_timeout(50U);
        else if (g_dfu.pending_op == DFU_PENDING_WRITE)
            dfu_set_poll_timeout(10U);
        else
            dfu_set_poll_timeout(1U);
    }
    else if (g_dfu.state == DFU_STATE_MANIFEST_SYNC)
    {
        g_dfu.state = DFU_STATE_MANIFEST;
        dfu_set_poll_timeout(1U);
    }

    dfu_send_status_response();
}

static void dfu_handle_getstate(void)
{
    uint8_t state = (uint8_t)g_dfu.state;
    ep0_send_response(&state, 1);
}

static void dfu_handle_clrstatus(void)
{
    if (g_dfu.state == DFU_STATE_ERROR)
    {
        g_dfu.state = DFU_STATE_IDLE;
        g_dfu.status = DFU_STATUS_OK;
        g_dfu.pending_op = DFU_PENDING_NONE;
    }
    ep0_send_zlp();
}

static void dfu_handle_abort(void)
{
    g_dfu.state = DFU_STATE_IDLE;
    g_dfu.status = DFU_STATUS_OK;
    g_dfu.pending_op = DFU_PENDING_NONE;
    g_dfu.ctrl_total_len = 0;
    g_dfu.ctrl_received = 0;
    g_dfu.data_len = 0;
    ep0_send_zlp();
}

static void dfu_handle_upload(void)
{
    static const uint8_t commands[] = {
        DFUSE_CMD_GET_COMMANDS,
        DFUSE_CMD_SET_ADDRESS,
        DFUSE_CMD_ERASE
    };
    const uint8_t *data = 0;
    uint16_t len = g_dfu.setup.wLength;
    uint16_t actual_len = 0;
    uint32_t read_addr;

    g_dfu.status = DFU_STATUS_OK;

    if (g_dfu.setup.wValue == 0U)
    {
        actual_len = sizeof(commands);
        if (actual_len > len)
            actual_len = len;
        ep0_send_response(commands, actual_len);
        g_dfu.state = (actual_len < len) ? DFU_STATE_IDLE : DFU_STATE_UPLOAD_IDLE;
        return;
    }

    if (g_dfu.setup.wValue < 2U)
    {
        dfu_set_error(DFU_STATUS_ERR_STALLEDPKT);
        ep0_stall();
        return;
    }

    read_addr = g_dfu.addr_ptr + (uint32_t)(g_dfu.setup.wValue - 2U) * DFU_XFER_SIZE;
    data = dfu_media_read(read_addr, len, &actual_len);
    if (data == 0)
    {
        ep0_send_zlp();
        g_dfu.state = DFU_STATE_IDLE;
        return;
    }

    ep0_send_response(data, actual_len);
    g_dfu.state = (actual_len < len) ? DFU_STATE_IDLE : DFU_STATE_UPLOAD_IDLE;
}

/* ---- Standard request handling ---- */
static void handle_get_descriptor(void)
{
    const uint8_t *desc = 0;
    uint16_t desc_len = 0;
    uint8_t dtype = (uint8_t)(g_dfu.setup.wValue >> 8);

    switch (dtype)
    {
        case USB_DESC_DEVICE:
            desc = dev_desc;
            desc_len = sizeof(dev_desc);
            break;

        case USB_DESC_CONFIGURATION:
            desc = cfg_desc;
            desc_len = sizeof(cfg_desc);
            break;

        case USB_DESC_DEVICE_QUALIFIER:
            desc = dev_qual_desc;
            desc_len = sizeof(dev_qual_desc);
            break;

        case USB_DESC_DFU_FUNCTIONAL:
            desc = dfu_func_desc;
            desc_len = sizeof(dfu_func_desc);
            break;

        case USB_DESC_STRING:
            desc = get_string_desc((uint8_t)(g_dfu.setup.wValue & 0xFFU), &desc_len);
            break;

        default:
            ep0_stall();
            return;
    }

    if (desc == 0)
    {
        ep0_stall();
        return;
    }

    if (desc_len > g_dfu.setup.wLength)
        desc_len = g_dfu.setup.wLength;

    ep0_send_response(desc, desc_len);
}

static void handle_set_address(void)
{
    g_dfu.pending_usb_addr = (uint8_t)(g_dfu.setup.wValue & 0x7FU);
    ep0_send_zlp();
}

static void handle_get_status_std(void)
{
    static const uint8_t status[2] = {0, 0};
    ep0_send_response(status, sizeof(status));
}

static void handle_get_configuration(void)
{
    uint8_t cfg = g_dfu.configured ? DFU_CONFIG_VALUE : 0U;
    ep0_send_response(&cfg, 1);
}

static void handle_set_configuration(void)
{
    g_dfu.configured = (g_dfu.setup.wValue == DFU_CONFIG_VALUE) ? 1U : 0U;
    if (g_dfu.configured)
        dfu_select_alt(DFU_ALT_APPLICATION);
    ep0_send_zlp();
}

static void handle_get_interface(void)
{
    ep0_send_response(&g_dfu.alt_setting, 1);
}

static void handle_set_interface(void)
{
    if (g_dfu.setup.wValue >= DFU_ALT_COUNT)
    {
        ep0_stall();
        return;
    }

    dfu_select_alt((uint8_t)g_dfu.setup.wValue);
    ep0_send_zlp();
}

static void handle_standard_request(void)
{
    switch (g_dfu.setup.bRequest)
    {
        case USB_REQ_GET_DESCRIPTOR:
            handle_get_descriptor();
            break;

        case USB_REQ_SET_ADDRESS:
            handle_set_address();
            break;

        case USB_REQ_GET_STATUS:
            handle_get_status_std();
            break;

        case USB_REQ_GET_CONFIGURATION:
            handle_get_configuration();
            break;

        case USB_REQ_SET_CONFIGURATION:
            handle_set_configuration();
            break;

        case USB_REQ_GET_INTERFACE:
            handle_get_interface();
            break;

        case USB_REQ_SET_INTERFACE:
            handle_set_interface();
            break;

        case USB_REQ_CLEAR_FEATURE:
            ep0_send_zlp();
            break;

        default:
            ep0_stall();
            break;
    }
}

static void handle_class_request(void)
{
    switch (g_dfu.setup.bRequest)
    {
        case DFU_DNLOAD:
            dfu_begin_dnload();
            break;

        case DFU_UPLOAD:
            dfu_handle_upload();
            break;

        case DFU_GETSTATUS:
            dfu_handle_getstatus();
            break;

        case DFU_GETSTATE:
            dfu_handle_getstate();
            break;

        case DFU_CLRSTATUS:
            dfu_handle_clrstatus();
            break;

        case DFU_ABORT:
            dfu_handle_abort();
            break;

        default:
            ep0_stall();
            break;
    }
}

static void handle_setup(void)
{
    pma_read(EP0_RX_ADDR, (uint8_t *)&g_dfu.setup, sizeof(g_dfu.setup));

    switch (g_dfu.setup.bmRequestType & 0x60U)
    {
        case 0x00U:
            handle_standard_request();
            break;

        case 0x20U:
            handle_class_request();
            break;

        default:
            ep0_stall();
            break;
    }
}

static void handle_ep0_rx_data(void)
{
    uint16_t count = USB_PMA(BDT_COUNT_RX) & 0x03FFU;
    uint16_t remaining = DFU_XFER_SIZE - g_dfu.ctrl_received;

    if (count > remaining)
        count = remaining;

    pma_read(EP0_RX_ADDR, &g_dfu.rx_buffer[g_dfu.ctrl_received], count);
    g_dfu.ctrl_received += count;

    if (g_dfu.ctrl_received >= g_dfu.ctrl_total_len)
    {
        g_dfu.data_len = g_dfu.ctrl_received;
        dfu_prepare_pending_op();
        ep0_send_zlp();
    }
    else
    {
        ep_set_rx_status(0, EP_RX_VALID);
    }
}

static void handle_ep0_tx_complete(void)
{
    if (g_dfu.pending_usb_addr != 0U)
    {
        USB_DADDR = 0x0080U | g_dfu.pending_usb_addr;
        g_dfu.pending_usb_addr = 0U;
    }

    if (g_dfu.ctrl_stage == CTRL_STAGE_DATA_IN && g_dfu.tx_remaining > 0U)
    {
        uint16_t chunk = g_dfu.tx_remaining;

        if (chunk > EP0_MAX_PACKET)
            chunk = EP0_MAX_PACKET;

        ep0_send_packet(g_dfu.tx_ptr, chunk);
        g_dfu.tx_ptr += chunk;
        g_dfu.tx_remaining -= chunk;
        return;
    }

    if (g_dfu.state == DFU_STATE_DNLOAD_BUSY)
    {
        dfu_execute_pending_op();
    }
    else if (g_dfu.state == DFU_STATE_MANIFEST)
    {
        g_dfu.state = DFU_STATE_MANIFEST_WAIT_RESET;
        g_dfu.status = DFU_STATUS_OK;
        g_dfu.pending_op = DFU_PENDING_LEAVE;
        dfu_execute_pending_op();
    }

    g_dfu.ctrl_stage = CTRL_STAGE_IDLE;
    ep_set_rx_status(0, EP_RX_VALID);
}

static void usb_reset(void)
{
    USB_BTABLE = 0;
    USB_PMA(BDT_ADDR_TX) = EP0_TX_ADDR;
    USB_PMA(BDT_COUNT_TX) = 0;
    USB_PMA(BDT_ADDR_RX) = EP0_RX_ADDR;
    USB_PMA(BDT_COUNT_RX) = 0x8400U;

    USB_EPR(0) = EP_TYPE_CONTROL | 0U;
    ep_set_tx_status(0, EP_TX_NAK);
    ep_set_rx_status(0, EP_RX_VALID);

    USB_DADDR = 0x0080U;
    g_dfu.configured = 0;
    dfu_reset_runtime_state();
}

/* ---- Public API ---- */
void usb_dfu_init(void)
{
    g_dfu.complete_flag = 0;

    /* Force USB re-enumeration by pulling D+ low for a short time. */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    GPIOA->CRH = (GPIOA->CRH & ~(0xFU << 16)) | (0x2U << 16);
    GPIOA->BRR = (1U << 12);
    for (volatile int i = 0; i < 200000; ++i)
    {}

    GPIOA->CRH = (GPIOA->CRH & ~(0xFU << 16)) | (0x4U << 16);

    RCC->APB1ENR |= RCC_APB1ENR_USBEN;
    USB_CNTR = 0x0003U;
    for (volatile int i = 0; i < 100; ++i)
    {}
    USB_CNTR = 0x0001U;
    for (volatile int i = 0; i < 100; ++i)
    {}
    USB_CNTR = 0x0000U;
    USB_ISTR = 0;

    dfu_reset_runtime_state();
    USB_CNTR = 0x8400U;
}

void usb_dfu_poll(void)
{
    uint16_t istr = USB_ISTR;

    if ((istr & USB_ISTR_RESET) != 0U)
    {
        USB_ISTR = (uint16_t)~USB_ISTR_RESET;
        usb_reset();
        return;
    }

    if ((istr & USB_ISTR_CTR) == 0U)
        return;

    if ((istr & 0x0FU) != 0U)
        return;

    {
        uint16_t epr = USB_EPR(0);

        if ((epr & EP_SETUP) != 0U)
        {
            ep_clear_ctr_rx(0);
            handle_setup();
        }
        else if ((epr & EP_CTR_RX) != 0U)
        {
            ep_clear_ctr_rx(0);
            if (g_dfu.ctrl_stage == CTRL_STAGE_DATA_OUT)
                handle_ep0_rx_data();
            else
                ep_set_rx_status(0, EP_RX_VALID);
        }

        if ((epr & EP_CTR_TX) != 0U)
        {
            ep_clear_ctr_tx(0);
            handle_ep0_tx_complete();
        }
    }
}

int usb_dfu_is_complete(void)
{
    return g_dfu.complete_flag;
}
