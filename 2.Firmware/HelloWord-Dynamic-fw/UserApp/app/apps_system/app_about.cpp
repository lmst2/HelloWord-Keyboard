#include "app_about.h"
#include "display/eink_canvas.h"
#include "comm/config_cache.h"

extern ConfigCache kbConfigCache;

void AppAbout::OnEinkRender(EinkCanvas& canvas)
{
    canvas.Clear(true);

    // Title
    canvas.DrawRect(0, 0, 128, 20, true, true);
    canvas.DrawText(20, 6, "Hello Word 75", false);

    int16_t y = 30;
    canvas.DrawText(5, y, "Hub FW: v1.0", true); y += 14;
    canvas.DrawText(5, y, "KB  FW: v1.0", true); y += 14;
    canvas.DrawText(5, y, "MCU: STM32F405", true); y += 14;
    canvas.DrawText(5, y, "KB:  STM32F103", true); y += 20;

    canvas.DrawHLine(10, y, 108, true); y += 8;

    canvas.DrawText(5, y, "Keyboard:", true); y += 12;
    canvas.DrawText(10, y, "82 keys + 6 touch", true); y += 12;
    canvas.DrawText(10, y, "104 RGB LEDs", true); y += 14;

    canvas.DrawText(5, y, "Hub:", true); y += 12;
    canvas.DrawText(10, y, "BLDC motor + knob", true); y += 12;
    canvas.DrawText(10, y, "OLED 128x32", true); y += 12;
    canvas.DrawText(10, y, "E-Ink 128x296", true); y += 14;

    if (kbConfigCache.IsValid()) {
        canvas.DrawText(5, y, "KB connected", true);
    } else {
        canvas.DrawText(5, y, "KB disconnected", true);
    }

    needsRefresh_ = false;
}
