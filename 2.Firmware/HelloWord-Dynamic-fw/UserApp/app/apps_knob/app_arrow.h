#ifndef APP_ARROW_H
#define APP_ARROW_H

#include "app/app_interface.h"

class AppArrowV : public IApp {
public:
    uint8_t     GetId() const override { return 0x09; }
    const char* GetName() const override { return "ArrowV"; }
    const uint8_t* GetIcon16() const override { return nullptr; }
    uint8_t GetFeatures() const override { return FEAT_KNOB; }

    KnobMotorMode GetMotorMode() const override { return KNOB_ENCODER; }
    void OnKnobDelta(int32_t delta) override;
};

class AppArrowH : public IApp {
public:
    uint8_t     GetId() const override { return 0x0A; }
    const char* GetName() const override { return "ArrowH"; }
    const uint8_t* GetIcon16() const override { return nullptr; }
    uint8_t GetFeatures() const override { return FEAT_KNOB; }

    KnobMotorMode GetMotorMode() const override { return KNOB_ENCODER; }
    void OnKnobDelta(int32_t delta) override;
};

#endif // APP_ARROW_H
