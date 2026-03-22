#ifndef APP_SCROLL_H
#define APP_SCROLL_H

#include "app/app_interface.h"

class AppScroll : public IApp {
public:
    uint8_t     GetId() const override { return 0x02; }
    const char* GetName() const override { return "Scroll"; }
    const uint8_t* GetIcon16() const override { return nullptr; }
    uint8_t GetFeatures() const override { return FEAT_KNOB; }

    KnobMotorMode GetMotorMode() const override { return motorMode_; }
    void OnKnobDelta(int32_t delta) override;

    uint8_t GetSubItemCount() const override { return 4; }
    const char* GetSubItemName(uint8_t idx) const override;
    uint8_t GetActiveSubItem() const override { return (uint8_t)motorMode_; }
    void OnSubItemSelected(uint8_t idx) override;

private:
    KnobMotorMode motorMode_ = KNOB_ENCODER;
};

#endif // APP_SCROLL_H
