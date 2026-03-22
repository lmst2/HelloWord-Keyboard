#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <stdint.h>

class PowerManager {
public:
    enum State { ACTIVE, DIM, STANDBY };

    void Init(uint32_t dimTimeoutMs = 30000, uint32_t standbyTimeoutMs = 60000);
    void OnActivity();
    void Tick(uint32_t nowMs);

    State GetState() const { return state_; }
    uint8_t GetOledBrightness() const;
    bool IsMotorEnabled() const { return state_ != STANDBY; }

private:
    State state_ = ACTIVE;
    uint32_t lastActivityMs_ = 0;
    uint32_t dimTimeoutMs_ = 30000;
    uint32_t standbyTimeoutMs_ = 60000;
};

extern PowerManager powerManager;

#endif // POWER_MANAGER_H
