#include "app_settings.h"
#include "display/eink_canvas.h"
#include "comm/config_cache.h"
#include "config/hub_config.h"

extern ConfigCache kbConfigCache;

struct MenuItem {
    const char* label;
    uint16_t paramId;
    uint8_t minVal;
    uint8_t maxVal;
};

static const MenuItem LIGHTING_ITEMS[] = {
    {"Effect Mode",    0x0100, 0, 4},
    {"Brightness",     0x0101, 0, 6},
    {"Effect Speed",   0x0106, 0, 255},
    {"Rainbow Hue",    0x0110, 0, 255},
    {"Reactive Hue",   0x0120, 0, 255},
    {"Reactive Sat",   0x0121, 0, 255},
    {"Aurora Tint",    0x0130, 0, 255},
    {"Ripple Hue",     0x0140, 0, 255},
    {"Static R",       0x0150, 0, 255},
    {"Static G",       0x0151, 0, 255},
    {"Static B",       0x0152, 0, 255},
};
static constexpr uint8_t LIGHTING_COUNT = sizeof(LIGHTING_ITEMS) / sizeof(LIGHTING_ITEMS[0]);

static const MenuItem TOUCHBAR_ITEMS[] = {
    {"TB Mode",        0x0200, 0, 2},
    {"Activation ms",  0x0201, 0, 255},
    {"Release Grace",  0x0202, 0, 255},
};
static constexpr uint8_t TOUCHBAR_COUNT = sizeof(TOUCHBAR_ITEMS) / sizeof(TOUCHBAR_ITEMS[0]);

static const MenuItem KEYMAP_ITEMS[] = {
    {"Active Layer",   0x0300, 1, 3},
    {"OS Mode",        0x0301, 0, 255},
};
static constexpr uint8_t KEYMAP_COUNT = sizeof(KEYMAP_ITEMS) / sizeof(KEYMAP_ITEMS[0]);

static const MenuItem SLEEP_ITEMS[] = {
    {"Timeout (min)",  0x0400, 1, 30},
    {"Fade ms",        0x0401, 0, 255},
    {"Breathe ms",     0x0402, 0, 255},
};
static constexpr uint8_t SLEEP_COUNT = sizeof(SLEEP_ITEMS) / sizeof(SLEEP_ITEMS[0]);

static const MenuItem HUB_ITEMS[] = {
    {"OLED Bright",    0x0500, 0, 255},
    {"Standby (s)",    0x0501, 0, 255},
    {"Torque Limit",   0x0510, 0, 100},
    {"Default PPR",    0x0511, 1, 48},
};
static constexpr uint8_t HUB_COUNT = sizeof(HUB_ITEMS) / sizeof(HUB_ITEMS[0]);

static const MenuItem* GetCategoryItems(uint8_t cat, uint8_t& count)
{
    switch (cat) {
        case 0: count = LIGHTING_COUNT; return LIGHTING_ITEMS;
        case 1: count = TOUCHBAR_COUNT; return TOUCHBAR_ITEMS;
        case 2: count = KEYMAP_COUNT;   return KEYMAP_ITEMS;
        case 3: count = SLEEP_COUNT;    return SLEEP_ITEMS;
        case 4: count = HUB_COUNT;      return HUB_ITEMS;
        default: count = 0; return nullptr;
    }
}

void AppSettings::OnKnobDelta(int32_t delta)
{
    scrollPos_ += (int16_t)delta;
    needsRefresh_ = true;
}

const char* AppSettings::GetSubItemName(uint8_t idx) const
{
    static const char* names[] = {
        "Lighting", "TouchBar", "Keymap", "Sleep", "Hub", "System"
    };
    return idx < 6 ? names[idx] : nullptr;
}

void AppSettings::OnSubItemSelected(uint8_t idx)
{
    if (idx < 6) {
        activeCategory_ = idx;
        scrollPos_ = 0;
        needsRefresh_ = true;
    }
}

void AppSettings::OnEinkRender(EinkCanvas& canvas)
{
    canvas.Clear(true);

    // Title
    const char* catName = GetSubItemName(activeCategory_);
    canvas.DrawText(5, 3, "Settings:", true);
    if (catName) canvas.DrawText(60, 3, catName, true);
    canvas.DrawHLine(0, 12, 128, true);

    uint8_t itemCount = 0;
    const MenuItem* items = GetCategoryItems(activeCategory_, itemCount);

    if (!items || itemCount == 0) {
        if (activeCategory_ == 5) {
            canvas.DrawText(10, 50, "FW: v1.0", true);
            canvas.DrawText(10, 65, "Hub: v1.0", true);
            canvas.DrawText(10, 80, "Hello Word 75", true);
        }
        needsRefresh_ = false;
        return;
    }

    // Clamp scroll position
    if (scrollPos_ < 0) scrollPos_ = 0;
    if (scrollPos_ >= itemCount) scrollPos_ = itemCount - 1;

    int16_t y = 18;
    int16_t lineH = 22;

    for (uint8_t i = 0; i < itemCount && y < (int16_t)EinkCanvas::HEIGHT - 10; i++) {
        bool selected = (i == scrollPos_);

        if (selected) {
            canvas.DrawRect(0, y - 2, 128, lineH, true, true);
        }

        // Label
        canvas.DrawText(3, y + 2, items[i].label, !selected);

        // Value
        uint8_t val[4];
        uint8_t vlen = 0;
        if (items[i].paramId >= 0x0500) {
            vlen = hubConfig.GetParam(items[i].paramId, val);
        } else {
            vlen = kbConfigCache.GetParam(items[i].paramId, val);
        }

        if (vlen > 0) {
            canvas.DrawNumber(95, y + 2, val[0], !selected);
        }

        y += lineH;
    }

    needsRefresh_ = false;
}
