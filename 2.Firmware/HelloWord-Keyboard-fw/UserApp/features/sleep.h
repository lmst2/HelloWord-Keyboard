#ifndef SLEEP_H
#define SLEEP_H

#include <stdint.h>
#include "HelloWord/hw_keyboard.h"
#include "configurations.h"

struct SleepState_t
{
    volatile bool isSleeping = false;
    volatile bool isFadeOutActive = false;
    volatile uint32_t lastActivityMs = 0;
    volatile uint32_t fadeStartMs = 0;
};

class SleepManager {
public:
    void Init(const SleepConfig_t* cfg) { cfg_ = cfg; }

    void Update(uint32_t nowMs, bool hasPhysicalInput);

    bool IsSleeping() const { return state_.isSleeping; }
    bool IsFading() const { return state_.isFadeOutActive; }

    // Apply dimming to main keyboard LEDs (not status LEDs)
    void ApplyLighting(HWKeyboard& kb, uint32_t nowMs);

    // Render status LED breathing pattern during sleep
    void RenderStatusLeds(HWKeyboard& kb, uint32_t nowMs,
                          uint8_t statusLedStart, uint8_t statusLedCount);

private:
    float GetFadeScale(uint32_t nowMs) const;
    uint16_t GetPulseLevel(uint32_t nowMs) const;
    uint8_t GetPulseRawBrightness(uint32_t nowMs) const;

    uint32_t GetIdleTimeoutMs() const;
    uint32_t GetFadeMs() const;
    uint32_t GetBreatheMs() const;

    SleepState_t state_{};
    const SleepConfig_t* cfg_ = nullptr;

    static constexpr uint16_t PULSE_RESOLUTION = 1024;
    static constexpr uint8_t STATUS_MAX_RAW_BRIGHTNESS = 48;
};

extern SleepManager sleepManager;

#endif // SLEEP_H
