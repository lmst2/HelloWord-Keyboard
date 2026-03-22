#include "oled_display.h"
#include <cstring>

OledDisplay oledDisplay;

void OledDisplay::Render(uint32_t nowMs)
{
    if (!u8g2_ || !mgr_) return;

    u8g2_->ClearBuffer();

    if (mgr_->IsInSubmenu()) {
        RenderSubmenuList();
    } else {
        IApp* primary = mgr_->GetPrimaryApp();
        if (primary) RenderAppName(primary->GetName());
        RenderAppList();
    }

    u8g2_->SendBuffer();
}

void OledDisplay::RenderAppName(const char* name)
{
    if (!name) return;
    // Draw app name at top, small font
    u8g2_->SetFont(u8g2_font_5x7_tr);
    u8g2_->DrawStr(1, 7, name);
    u8g2_->DrawHLine(0, 9, 32);
}

void OledDisplay::RenderAppList()
{
    uint8_t visCount = mgr_->GetVisibleAppCount();
    if (visCount == 0) return;

    uint8_t activeVisIdx = mgr_->GetVisiblePrimaryIndex();
    uint8_t startY = 12;
    uint8_t itemH = 16;
    uint8_t maxVisible = 6;

    // Calculate scroll offset to keep active item centered
    int16_t scrollStart = (int16_t)activeVisIdx - (int16_t)(maxVisible / 2);
    if (scrollStart < 0) scrollStart = 0;
    if (scrollStart + maxVisible > visCount) scrollStart = visCount > maxVisible ? visCount - maxVisible : 0;

    u8g2_->SetFont(u8g2_font_5x7_tr);

    for (uint8_t i = 0; i < maxVisible && (scrollStart + i) < visCount; i++) {
        IApp* app = mgr_->GetVisibleApp(scrollStart + i);
        if (!app) continue;

        uint8_t y = startY + i * itemH;
        bool isActive = ((uint8_t)(scrollStart + i) == activeVisIdx);

        if (isActive) {
            // Highlighted: inverted colors
            u8g2_->SetDrawColor(1);
            u8g2_->DrawBox(0, y, 32, itemH);
            u8g2_->SetDrawColor(0);
        } else {
            u8g2_->SetDrawColor(1);
        }

        // Draw icon (placeholder: first 2 chars of name)
        const char* name = app->GetName();
        char abbr[3] = {name[0], name[1], '\0'};
        u8g2_->DrawStr(2, y + itemH - 4, abbr);

        u8g2_->SetDrawColor(1);
    }
}

void OledDisplay::RenderSubmenuList()
{
    IApp* app = mgr_->GetPrimaryApp();
    if (!app) return;

    uint8_t count = app->GetSubItemCount();
    if (count == 0) return;

    u8g2_->SetFont(u8g2_font_5x7_tr);
    u8g2_->DrawStr(1, 7, "Sub");
    u8g2_->DrawHLine(0, 9, 32);

    uint8_t activeIdx = app->GetActiveSubItem();
    uint8_t startY = 12;
    uint8_t itemH = 14;

    for (uint8_t i = 0; i < count && i < 7; i++) {
        uint8_t y = startY + i * itemH;
        const char* name = app->GetSubItemName(i);
        if (!name) continue;

        bool isActive = (i == activeIdx);

        if (isActive) {
            u8g2_->SetDrawColor(1);
            u8g2_->DrawBox(0, y, 32, itemH);
            u8g2_->SetDrawColor(0);
        } else {
            u8g2_->SetDrawColor(1);
        }

        char abbr[4] = {name[0], name[1], name[2], '\0'};
        u8g2_->DrawStr(2, y + itemH - 3, abbr);
        u8g2_->SetDrawColor(1);
    }
}

void OledDisplay::RenderKnobIndicator(float knobPos, bool active)
{
    // Knob position indicator bar at bottom (will be expanded later)
    if (!active) return;
    uint8_t barY = 120;
    uint8_t barLen = (uint8_t)(knobPos * 32.0f);
    if (barLen > 32) barLen = 32;
    u8g2_->DrawBox(0, barY, barLen, 4);
}
