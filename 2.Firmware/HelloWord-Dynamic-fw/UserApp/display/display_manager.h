#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <stdint.h>
#include "eink_canvas.h"
#include "eink_driver.h"
#include "oled_display.h"
#include "app/app_manager.h"

class DisplayManager {
public:
    void Init(EinkDriver* einkDrv, OledDisplay* oledDisp, AppManager* mgr);

    void TickOled(uint32_t nowMs);
    void TickEink(uint32_t nowMs);

    EinkCanvas& GetCanvas() { return canvas_; }

private:
    EinkDriver* einkDrv_ = nullptr;
    OledDisplay* oledDisp_ = nullptr;
    AppManager* mgr_ = nullptr;
    EinkCanvas canvas_;
    uint32_t lastEinkRefreshMs_ = 0;
    bool einkFirstRender_ = true;

    static constexpr uint32_t EINK_MIN_REFRESH_INTERVAL_MS = 2000;
};

extern DisplayManager displayManager;

#endif // DISPLAY_MANAGER_H
