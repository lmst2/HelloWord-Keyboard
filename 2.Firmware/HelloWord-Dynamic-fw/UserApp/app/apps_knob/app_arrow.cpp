#include "app_arrow.h"
#include "comm/hub_uart_comm.h"
#include "comm/protocol.h"

extern HubUartComm hubUart;

void AppArrowV::OnKnobDelta(int32_t delta)
{
    if (delta == 0) return;
    uint8_t keycode = delta > 0 ? 0x52 : 0x51; // UP / DOWN arrow
    hubUart.SendKeyAction(Msg::KEY_ACTION_TAP, 0, keycode, 2);
}

void AppArrowH::OnKnobDelta(int32_t delta)
{
    if (delta == 0) return;
    uint8_t keycode = delta > 0 ? 0x4F : 0x50; // RIGHT / LEFT arrow
    hubUart.SendKeyAction(Msg::KEY_ACTION_TAP, 0, keycode, 2);
}
