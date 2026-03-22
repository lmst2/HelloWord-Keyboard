#include "app_volume.h"
#include "comm/hub_uart_comm.h"
#include "comm/protocol.h"

extern HubUartComm hubUart;

void AppVolume::OnKnobDelta(int32_t delta)
{
    if (delta == 0) return;

    // Send volume up/down as HID key
    uint8_t keycode = delta > 0 ? 0x80 : 0x81; // VOLUME_UP / VOLUME_DOWN
    for (int32_t i = 0; i < (delta > 0 ? delta : -delta); i++) {
        hubUart.SendKeyAction(Msg::KEY_ACTION_TAP, 0, keycode, 2);
    }
}

const char* AppVolume::GetSubItemName(uint8_t idx) const
{
    static const char* names[] = {"Disable", "Inertia", "Encoder", "Spring"};
    return idx < 4 ? names[idx] : nullptr;
}

void AppVolume::OnSubItemSelected(uint8_t idx)
{
    if (idx < 4) motorMode_ = (KnobMotorMode)idx;
}
