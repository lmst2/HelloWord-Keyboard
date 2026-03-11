#ifndef USB_HID_H
#define USB_HID_H

#include <stdint.h>

#define APP_ADDRESS         0x08004000U
#define FLASH_START_ADDRESS 0x08000000U
#define FLASH_END_ADDRESS   0x08020000U
#define FLASH_PAGE_SIZE     1024U
#define APP_SIZE            (FLASH_END_ADDRESS - APP_ADDRESS)

void usb_hid_init(void);
void usb_hid_poll(void);
int  usb_hid_is_complete(void);

#endif
