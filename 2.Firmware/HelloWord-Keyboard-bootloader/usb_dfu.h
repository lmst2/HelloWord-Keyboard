#ifndef USB_DFU_H
#define USB_DFU_H

#include <stdint.h>

#define APP_ADDRESS                 0x08004000U
#define BOOTLOADER_SIZE             0x00004000U
#define FLASH_START_ADDRESS         0x08000000U
#define FLASH_END_ADDRESS           0x08020000U
#define FLASH_PAGE_SIZE             1024U
#define DFU_XFER_SIZE               FLASH_PAGE_SIZE

#define USB_VID                     0x0483U
#define USB_PID                     0xDF11U
#define USB_LANGID_EN_US            0x0409U

#define DFU_CONFIG_VALUE            1U
#define DFU_ALT_INTERFACE_COUNT     1U
#define DFU_DEFAULT_ALT_SETTING     0U

/* USB standard requests */
#define USB_REQ_GET_STATUS          0U
#define USB_REQ_CLEAR_FEATURE       1U
#define USB_REQ_SET_ADDRESS         5U
#define USB_REQ_GET_DESCRIPTOR      6U
#define USB_REQ_GET_CONFIGURATION   8U
#define USB_REQ_SET_CONFIGURATION   9U
#define USB_REQ_GET_INTERFACE       10U
#define USB_REQ_SET_INTERFACE       11U

/* USB descriptor types */
#define USB_DESC_DEVICE             1U
#define USB_DESC_CONFIGURATION      2U
#define USB_DESC_STRING             3U
#define USB_DESC_INTERFACE          4U
#define USB_DESC_DEVICE_QUALIFIER   6U
#define USB_DESC_DFU_FUNCTIONAL     0x21U

/* DFU class requests */
#define DFU_DETACH                  0U
#define DFU_DNLOAD                  1U
#define DFU_UPLOAD                  2U
#define DFU_GETSTATUS               3U
#define DFU_CLRSTATUS               4U
#define DFU_GETSTATE                5U
#define DFU_ABORT                   6U

/* DfuSe commands */
#define DFUSE_CMD_GET_COMMANDS      0x00U
#define DFUSE_CMD_SET_ADDRESS       0x21U
#define DFUSE_CMD_ERASE             0x41U

typedef enum
{
    DFU_STATE_APP_IDLE = 0,
    DFU_STATE_APP_DETACH = 1,
    DFU_STATE_IDLE = 2,
    DFU_STATE_DNLOAD_SYNC = 3,
    DFU_STATE_DNLOAD_BUSY = 4,
    DFU_STATE_DNLOAD_IDLE = 5,
    DFU_STATE_MANIFEST_SYNC = 6,
    DFU_STATE_MANIFEST = 7,
    DFU_STATE_MANIFEST_WAIT_RESET = 8,
    DFU_STATE_UPLOAD_IDLE = 9,
    DFU_STATE_ERROR = 10
} dfu_state_t;

typedef enum
{
    DFU_STATUS_OK = 0x00,
    DFU_STATUS_ERR_TARGET = 0x01,
    DFU_STATUS_ERR_FILE = 0x02,
    DFU_STATUS_ERR_WRITE = 0x03,
    DFU_STATUS_ERR_ERASE = 0x04,
    DFU_STATUS_ERR_CHECK_ERASED = 0x05,
    DFU_STATUS_ERR_PROG = 0x06,
    DFU_STATUS_ERR_VERIFY = 0x07,
    DFU_STATUS_ERR_ADDRESS = 0x08,
    DFU_STATUS_ERR_NOTDONE = 0x09,
    DFU_STATUS_ERR_FIRMWARE = 0x0A,
    DFU_STATUS_ERR_VENDOR = 0x0B,
    DFU_STATUS_ERR_USB = 0x0C,
    DFU_STATUS_ERR_POR = 0x0D,
    DFU_STATUS_ERR_UNKNOWN = 0x0E,
    DFU_STATUS_ERR_STALLEDPKT = 0x0F
} dfu_status_t;

void usb_dfu_init(void);
void usb_dfu_poll(void);
int usb_dfu_is_complete(void);

#endif
