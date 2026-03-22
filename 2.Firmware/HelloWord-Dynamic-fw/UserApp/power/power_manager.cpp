#include "power_manager.h"

PowerManager powerManager;

void PowerManager::Init(uint32_t dimTimeoutMs, uint32_t standbyTimeoutMs)
{
    dimTimeoutMs_ = dimTimeoutMs;
    standbyTimeoutMs_ = standbyTimeoutMs;
    state_ = ACTIVE;
    lastActivityMs_ = 0;
}

void PowerManager::OnActivity()
{
    state_ = ACTIVE;
    lastActivityMs_ = 0; // Will be set by next Tick
}

void PowerManager::Tick(uint32_t nowMs)
{
    if (lastActivityMs_ == 0) lastActivityMs_ = nowMs;

    uint32_t idle = nowMs - lastActivityMs_;

    if (idle >= standbyTimeoutMs_) {
        state_ = STANDBY;
    } else if (idle >= dimTimeoutMs_) {
        state_ = DIM;
    } else {
        state_ = ACTIVE;
    }
}

uint8_t PowerManager::GetOledBrightness() const
{
    switch (state_) {
        case ACTIVE:  return 255;
        case DIM:     return 64;
        case STANDBY: return 0;
        default:      return 255;
    }
}
