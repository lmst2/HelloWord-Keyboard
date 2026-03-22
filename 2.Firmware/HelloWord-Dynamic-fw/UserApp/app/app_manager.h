#ifndef APP_MANAGER_H
#define APP_MANAGER_H

#include "app_interface.h"
#include <stdint.h>

class AppManager {
public:
    void RegisterApp(IApp* app);

    // ---- Primary app (owns knob + motor) ----
    void SetPrimaryApp(uint8_t appId);
    void NextPrimaryApp();
    void PrevPrimaryApp();
    IApp* GetPrimaryApp() const;
    uint8_t GetPrimaryIndex() const { return primaryIdx_; }

    // ---- E-Ink app (independent from primary) ----
    void SetEinkApp(uint8_t appId);
    void NextEinkApp();
    void PrevEinkApp();
    IApp* GetEinkApp() const;

    // ---- L2 submenu ----
    void EnterSubmenu();
    void ExitSubmenu();
    bool IsInSubmenu() const { return inSubmenu_; }
    void NextSubItem();
    void PrevSubItem();
    uint8_t GetSubmenuTarget() const { return submenuTarget_; }

    // ---- OS filtering ----
    void SetOsMode(uint8_t os);
    uint8_t GetVisibleAppCount() const;
    IApp* GetVisibleApp(uint8_t visibleIdx) const;
    uint8_t GetVisiblePrimaryIndex() const;

    // ---- Dispatch ----
    void OnKnobDelta(int32_t delta);
    void OnPcData(uint8_t feedId, const uint8_t* data, uint8_t len);
    void Tick(uint32_t nowMs);

    // ---- Accessors ----
    uint8_t GetAppCount() const { return appCount_; }
    IApp* GetApp(uint8_t idx) const { return idx < appCount_ ? apps_[idx] : nullptr; }

private:
    int16_t FindVisibleIndex(uint8_t rawIdx) const;
    int16_t NextVisibleFrom(uint8_t rawIdx, int8_t direction) const;
    bool IsVisible(IApp* app) const;

    IApp* apps_[24]{};
    uint8_t appCount_ = 0;
    uint8_t primaryIdx_ = 0;
    uint8_t einkIdx_ = 0;
    uint8_t osMode_ = OS_BOTH;
    bool inSubmenu_ = false;
    uint8_t submenuTarget_ = 0;
    uint8_t subItemCursor_ = 0;
};

extern AppManager appManager;

#endif // APP_MANAGER_H
