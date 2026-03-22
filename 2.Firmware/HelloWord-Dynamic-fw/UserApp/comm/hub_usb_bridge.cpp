#include "hub_usb_comm.h"

extern "C" void HubUsb_OnCdcData(const uint8_t* data, uint16_t len)
{
    hubUsb.OnCdcData(data, len);
}
