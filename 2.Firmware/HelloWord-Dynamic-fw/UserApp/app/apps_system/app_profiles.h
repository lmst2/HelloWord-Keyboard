#ifndef APP_PROFILES_H
#define APP_PROFILES_H

#include "app/app_interface.h"
#include "config/profile_store.h"

class AppProfiles : public IApp {
public:
    uint8_t     GetId() const override { return 0xFC; }
    const char* GetName() const override { return "Prof"; }
    const uint8_t* GetIcon16() const override { return nullptr; }
    uint8_t GetFeatures() const override { return FEAT_KNOB; }

    KnobMotorMode GetMotorMode() const override { return KNOB_ENCODER; }

    void OnKnobDelta(int32_t delta) override;

    // L2: profile slot list
    uint8_t GetSubItemCount() const override;
    const char* GetSubItemName(uint8_t idx) const override;
    uint8_t GetActiveSubItem() const override { return selectedSlot_; }
    void OnSubItemSelected(uint8_t idx) override;

private:
    uint8_t selectedSlot_ = 0;
};

#endif // APP_PROFILES_H
