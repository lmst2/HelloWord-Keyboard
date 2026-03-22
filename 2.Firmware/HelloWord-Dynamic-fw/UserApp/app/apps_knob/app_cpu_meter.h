#ifndef APP_CPU_METER_H
#define APP_CPU_METER_H

#include "app/app_interface.h"

class AppCpuMeter : public IApp {
public:
    uint8_t     GetId() const override { return 0x08; }
    const char* GetName() const override { return "CpuM"; }
    const uint8_t* GetIcon16() const override { return nullptr; }
    uint8_t GetFeatures() const override { return FEAT_KNOB | FEAT_MOTOR | FEAT_PC; }

    KnobMotorMode GetMotorMode() const override { return KNOB_SPRING; }
    float GetTorqueLimit() const override { return 0.8f; }

    void OnPcData(uint8_t feedId, const uint8_t* data, uint8_t len) override;
    void OnMotorTick(uint32_t nowMs) override;

    // L2: data source selection
    uint8_t GetSubItemCount() const override { return 3; }
    const char* GetSubItemName(uint8_t idx) const override;
    uint8_t GetActiveSubItem() const override { return dataSource_; }
    void OnSubItemSelected(uint8_t idx) override;

    float GetTargetAngle() const { return targetAngle_; }

private:
    float targetAngle_ = 0;
    uint8_t dataSource_ = 0;
    uint8_t currentValue_ = 0;

    static constexpr float ANGLE_MIN = 0.5f;
    static constexpr float ANGLE_MAX = 5.5f;

    static constexpr uint8_t FEED_ID_CPU = 0x01;
    static constexpr uint8_t FEED_ID_RAM = 0x02;
    static constexpr uint8_t FEED_ID_GPU = 0x03;
};

#endif // APP_CPU_METER_H
