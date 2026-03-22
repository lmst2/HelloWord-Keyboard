#ifndef USB_COMM_H
#define USB_COMM_H

#include <stdint.h>

class UsbComm {
public:
    // Called from HID_RxCpltCallback (ISR context)
    void OnHidReport(uint8_t* data);

    // Send a raw HID report (Report ID 2, max 32 bytes)
    bool SendRawReport(const uint8_t* data, uint8_t len);

private:
    void HandleRawCommand(uint8_t* data);
    void HandleConfigGet(uint8_t* data);
    void HandleConfigSet(uint8_t* data);
    void HandleConfigGetAll();
    void HandleStatusReq();
};

extern UsbComm usbComm;

#endif // USB_COMM_H
