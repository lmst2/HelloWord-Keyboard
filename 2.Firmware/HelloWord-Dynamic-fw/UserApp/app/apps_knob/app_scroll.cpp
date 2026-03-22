#include "app_scroll.h"
#include "comm/hub_uart_comm.h"
#include "comm/protocol.h"

extern HubUartComm hubUart;

void AppScroll::OnKnobDelta(int32_t delta)
{
    if (delta == 0) return;
    // Mouse wheel scroll via keyboard HID: modifier=0, keycode=0, but use key action for mouse
    // For mouse wheel, we send UP/DOWN arrow as a simple proxy
    // The keyboard firmware will inject these as HID keycodes
    uint8_t keycode = delta > 0 ? 0x52 : 0x51; // UP_ARROW / DOWN_ARROW
    hubUart.SendKeyAction(Msg::KEY_ACTION_TAP, 0, keycode, 2);
}

const char* AppScroll::GetSubItemName(uint8_t idx) const
{
    static const char* names[] = {"Disable", "Inertia", "Encoder", "Spring"};
    return idx < 4 ? names[idx] : nullptr;
}

void AppScroll::OnSubItemSelected(uint8_t idx)
{
    if (idx < 4) motorMode_ = (KnobMotorMode)idx;
}
