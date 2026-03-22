#include "app_switch_app.h"
#include "comm/hub_uart_comm.h"
#include "comm/protocol.h"

extern HubUartComm hubUart;

void AppSwitchApp::OnKnobDelta(int32_t delta)
{
    if (delta == 0) return;

    // Alt+Tab (Windows) / Cmd+Tab (Mac)
    // modifier: LEFT_ALT=0x04; keycode: TAB=0x2B
    uint8_t modifier = 0x04; // LEFT_ALT
    if (delta > 0) {
        hubUart.SendKeyAction(Msg::KEY_ACTION_TAP, modifier, 0x2B, 3);
    } else {
        // Alt+Shift+Tab for reverse
        hubUart.SendKeyAction(Msg::KEY_ACTION_TAP, modifier | 0x02, 0x2B, 3);
    }
}
