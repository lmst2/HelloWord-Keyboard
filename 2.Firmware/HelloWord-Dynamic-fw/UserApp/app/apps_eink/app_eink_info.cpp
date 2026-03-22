#include "app_eink_info.h"
#include "display/eink_canvas.h"
#include <string.h>

void AppEinkInfoPanel::OnPcData(uint8_t feedId, const uint8_t* data, uint8_t len)
{
    switch (feedId) {
        case FEED_ID_WEATHER:
            if (len > 0) { memcpy(weatherLine_, data, len < 31 ? len : 31); weatherLine_[len < 31 ? len : 31] = '\0'; }
            needsRefresh_ = true;
            break;
        case FEED_ID_DATE:
            if (len > 0) { memcpy(dateLine_, data, len < 31 ? len : 31); dateLine_[len < 31 ? len : 31] = '\0'; }
            needsRefresh_ = true;
            break;
        case FEED_ID_CALENDAR:
            if (len > 0) { memcpy(calendarLine_, data, len < 63 ? len : 63); calendarLine_[len < 63 ? len : 63] = '\0'; }
            needsRefresh_ = true;
            break;
        default: break;
    }
}

void AppEinkInfoPanel::OnEinkRender(EinkCanvas& canvas)
{
    canvas.Clear(true);
    canvas.DrawText(25, 5, "Info Panel", true);
    canvas.DrawHLine(0, 14, 128, true);

    int16_t y = 24;
    if (dateLine_[0]) {
        canvas.DrawText(2, y, dateLine_, true);
        y += 20;
    }
    if (weatherLine_[0]) {
        canvas.DrawText(2, y, "Weather:", true);
        canvas.DrawText(2, y + 10, weatherLine_, true);
        y += 30;
    }
    if (calendarLine_[0]) {
        canvas.DrawText(2, y, "Calendar:", true);
        canvas.DrawText(2, y + 10, calendarLine_, true);
    }

    needsRefresh_ = false;
}
