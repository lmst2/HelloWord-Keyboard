#ifndef APP_SWITCH_APP_H
#define APP_SWITCH_APP_H

#include "app/app_interface.h"

class AppSwitchApp : public IApp {
public:
    uint8_t     GetId() const override { return 0x03; }
    const char* GetName() const override { return "AppSw"; }
    const uint8_t* GetIcon16() const override { return nullptr; }
    uint8_t GetFeatures() const override { return FEAT_KNOB; }

    KnobMotorMode GetMotorMode() const override { return KNOB_SPRING; }
    void OnKnobDelta(int32_t delta) override;
};

#endif // APP_SWITCH_APP_H
