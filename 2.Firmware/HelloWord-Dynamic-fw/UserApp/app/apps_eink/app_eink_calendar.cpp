#include "app_eink_calendar.h"
#include "display/eink_canvas.h"
#include <stdio.h>

void AppEinkCalendar::OnPcData(uint8_t feedId, const uint8_t* data, uint8_t len)
{
    if (feedId != FEED_ID_DATE_INFO || len < 4) return;
    year_ = ((uint16_t)data[0] << 8) | data[1];
    month_ = data[2];
    day_ = data[3];
    weekday_ = DayOfWeek(year_, month_, day_);
    needsRefresh_ = true;
}

uint8_t AppEinkCalendar::DaysInMonth(uint16_t y, uint8_t m) const
{
    static const uint8_t d[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (m < 1 || m > 12) return 30;
    uint8_t days = d[m - 1];
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) days = 29;
    return days;
}

uint8_t AppEinkCalendar::DayOfWeek(uint16_t y, uint8_t m, uint8_t d) const
{
    // Tomohiko Sakamoto's algorithm (0=Sun, 6=Sat)
    static const int t[] = {0,3,2,5,0,3,5,1,4,6,2,4};
    if (m < 3) y--;
    return (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
}

void AppEinkCalendar::OnEinkRender(EinkCanvas& canvas)
{
    canvas.Clear(true);

    // Header: month/year
    char header[20];
    static const char* monthNames[] = {
        "Jan","Feb","Mar","Apr","May","Jun",
        "Jul","Aug","Sep","Oct","Nov","Dec"
    };
    const char* mn = (month_ >= 1 && month_ <= 12) ? monthNames[month_ - 1] : "???";
    snprintf(header, sizeof(header), "%s %d", mn, year_);
    canvas.DrawText(25, 5, header, true);
    canvas.DrawHLine(0, 14, 128, true);

    // Day names header
    static const char* dayNames[] = {"Su","Mo","Tu","We","Th","Fr","Sa"};
    for (int i = 0; i < 7; i++)
        canvas.DrawText(2 + i * 18, 20, dayNames[i], true);
    canvas.DrawHLine(0, 28, 128, true);

    // Calendar grid
    uint8_t firstDay = DayOfWeek(year_, month_, 1);
    uint8_t totalDays = DaysInMonth(year_, month_);
    int16_t row = 0;
    int16_t col = firstDay;

    for (uint8_t d = 1; d <= totalDays; d++) {
        int16_t x = 2 + col * 18;
        int16_t y = 32 + row * 14;

        if (d == day_) {
            canvas.DrawRect(x - 1, y - 1, 14, 10, true, true);
            canvas.DrawNumber(x, y, d, false);
        } else {
            canvas.DrawNumber(x, y, d, true);
        }

        col++;
        if (col >= 7) { col = 0; row++; }
    }

    needsRefresh_ = false;
}
