#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include "app/app_interface.h"

class AppSettings : public IApp {
public:
    uint8_t     GetId() const override { return 0xFD; }
    const char* GetName() const override { return "Setup"; }
    const uint8_t* GetIcon16() const override { return nullptr; }
    uint8_t GetFeatures() const override { return FEAT_KNOB | FEAT_EINK; }

    KnobMotorMode GetMotorMode() const override { return KNOB_ENCODER; }

    void OnKnobDelta(int32_t delta) override;

    // L2: config categories
    uint8_t GetSubItemCount() const override { return 6; }
    const char* GetSubItemName(uint8_t idx) const override;
    uint8_t GetActiveSubItem() const override { return activeCategory_; }
    void OnSubItemSelected(uint8_t idx) override;

    void OnEinkRender(EinkCanvas& canvas) override;
    bool NeedsEinkRefresh() const override { return needsRefresh_; }

private:
    uint8_t activeCategory_ = 0;
    int16_t scrollPos_ = 0;
    bool needsRefresh_ = false;
};

#endif // APP_SETTINGS_H
