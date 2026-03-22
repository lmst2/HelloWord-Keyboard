#include "app_cpu_meter.h"

void AppCpuMeter::OnPcData(uint8_t feedId, const uint8_t* data, uint8_t len)
{
    if (len < 1) return;

    uint8_t targetFeed = FEED_ID_CPU;
    switch (dataSource_) {
        case 0: targetFeed = FEED_ID_CPU; break;
        case 1: targetFeed = FEED_ID_RAM; break;
        case 2: targetFeed = FEED_ID_GPU; break;
    }

    if (feedId == targetFeed) {
        currentValue_ = data[0];
        targetAngle_ = ANGLE_MIN + (ANGLE_MAX - ANGLE_MIN) * (float)currentValue_ / 100.0f;
    }
}

void AppCpuMeter::OnMotorTick(uint32_t nowMs)
{
    // Motor position is controlled externally via GetTargetAngle()
    // The control loop reads this and sets motor.target = targetAngle_
    (void)nowMs;
}

const char* AppCpuMeter::GetSubItemName(uint8_t idx) const
{
    static const char* names[] = {"CPU", "RAM", "GPU"};
    return idx < 3 ? names[idx] : nullptr;
}

void AppCpuMeter::OnSubItemSelected(uint8_t idx)
{
    if (idx < 3) { dataSource_ = idx; currentValue_ = 0; targetAngle_ = ANGLE_MIN; }
}
