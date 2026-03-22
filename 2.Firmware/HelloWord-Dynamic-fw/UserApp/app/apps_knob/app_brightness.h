#ifndef APP_BRIGHTNESS_H
#define APP_BRIGHTNESS_H

#include "app/app_interface.h"

class AppBrightness : public IApp {
public:
    uint8_t     GetId() const override { return 0x05; }
    const char* GetName() const override { return "Bright"; }
    const uint8_t* GetIcon16() const override { return nullptr; }
    uint8_t GetFeatures() const override { return FEAT_KNOB; }

    KnobMotorMode GetMotorMode() const override { return KNOB_ENCODER; }
    void OnKnobDelta(int32_t delta) override;
};

#endif // APP_BRIGHTNESS_H
