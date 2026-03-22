#include "app_brightness.h"
#include "comm/hub_uart_comm.h"
#include "comm/protocol.h"

extern HubUartComm hubUart;

void AppBrightness::OnKnobDelta(int32_t delta)
{
    if (delta == 0) return;
    // Brightness up/down media keys
    uint8_t keycode = delta > 0 ? 0x6F : 0x70; // F24=brightness up, approximate
    hubUart.SendKeyAction(Msg::KEY_ACTION_TAP, 0, keycode, 2);
}
