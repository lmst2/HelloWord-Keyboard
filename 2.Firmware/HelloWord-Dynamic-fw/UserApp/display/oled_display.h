#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include <stdint.h>
#include "U8g2lib.hpp"
#include "app/app_manager.h"

class OledDisplay {
public:
    void Init(U8G2* u8g2, AppManager* mgr) { u8g2_ = u8g2; mgr_ = mgr; }

    void Render(uint32_t nowMs);

private:
    void RenderAppList();
    void RenderSubmenuList();
    void RenderAppName(const char* name);
    void RenderKnobIndicator(float knobPos, bool active);

    U8G2* u8g2_ = nullptr;
    AppManager* mgr_ = nullptr;
    float lastKnobPos_ = 0;
    uint32_t lastKnobActivityMs_ = 0;
};

extern OledDisplay oledDisplay;

#endif // OLED_DISPLAY_H
