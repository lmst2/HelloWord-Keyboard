#ifndef USB_DFU_H
#define USB_DFU_H

#include <stdint.h>

#define APP_ADDRESS         0x08004000U
#define BOOTLOADER_SIZE     0x4000U
#define FLASH_PAGE_SIZE     1024U
#define DFU_XFER_SIZE       FLASH_PAGE_SIZE

#define USB_VID             0x0483U
#define USB_PID             0xDF11U

/* DFU Class Requests */
#define DFU_DETACH          0
#define DFU_DNLOAD          1
#define DFU_UPLOAD          2
#define DFU_GETSTATUS       3
#define DFU_CLRSTATUS       4
#define DFU_GETSTATE        5
#define DFU_ABORT           6

typedef enum {
    STATE_DFU_IDLE          = 2,
    STATE_DFU_DNLOAD_SYNC   = 3,
    STATE_DFU_DNBUSY        = 4,
    STATE_DFU_DNLOAD_IDLE   = 5,
    STATE_DFU_MANIFEST_SYNC = 6,
    STATE_DFU_MANIFEST      = 7,
    STATE_DFU_ERROR         = 10
} dfu_state_t;

typedef enum {
    STATUS_OK               = 0x00,
    STATUS_ERR_TARGET       = 0x01,
    STATUS_ERR_WRITE        = 0x03,
    STATUS_ERR_ERASE        = 0x04,
    STATUS_ERR_ADDRESS      = 0x08,
    STATUS_ERR_UNKNOWN      = 0x0E,
    STATUS_ERR_STALLEDPKT   = 0x0F
} dfu_status_t;

/* USB Standard Request Codes */
#define USB_REQ_GET_STATUS          0
#define USB_REQ_CLEAR_FEATURE       1
#define USB_REQ_SET_ADDRESS         5
#define USB_REQ_GET_DESCRIPTOR      6
#define USB_REQ_GET_CONFIGURATION   8
#define USB_REQ_SET_CONFIGURATION   9

/* Descriptor Types */
#define USB_DESC_DEVICE             1
#define USB_DESC_CONFIGURATION      2
#define USB_DESC_STRING             3
#define USB_DESC_INTERFACE          4
#define USB_DESC_DFU_FUNCTIONAL     0x21

void usb_dfu_init(void);
void usb_dfu_poll(void);
int  usb_dfu_is_complete(void);

#endif
