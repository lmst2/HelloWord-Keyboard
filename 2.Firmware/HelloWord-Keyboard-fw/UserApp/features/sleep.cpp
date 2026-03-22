#include "sleep.h"
#include "effects/light_effects.h"

SleepManager sleepManager;

uint32_t SleepManager::GetIdleTimeoutMs() const
{
    if (!cfg_ || cfg_->timeoutMin == 0) return 300000;
    return (uint32_t)cfg_->timeoutMin * 60000;
}

uint32_t SleepManager::GetFadeMs() const
{
    return cfg_ ? cfg_->fadeMs : 800;
}

uint32_t SleepManager::GetBreatheMs() const
{
    return cfg_ ? cfg_->breatheMs : 4800;
}

void SleepManager::Update(uint32_t nowMs, bool hasPhysicalInput)
{
    if (hasPhysicalInput) {
        state_.lastActivityMs = nowMs;
        state_.isFadeOutActive = false;
        state_.isSleeping = false;
        return;
    }

    if (state_.isSleeping) return;

    if (state_.isFadeOutActive) {
        if (nowMs - state_.fadeStartMs >= GetFadeMs()) {
            state_.isFadeOutActive = false;
            state_.isSleeping = true;
        }
        return;
    }

    if (nowMs - state_.lastActivityMs >= GetIdleTimeoutMs()) {
        state_.isFadeOutActive = true;
        state_.fadeStartMs = nowMs;
    }
}

float SleepManager::GetFadeScale(uint32_t nowMs) const
{
    if (state_.isSleeping) return 0.0f;
    if (!state_.isFadeOutActive) return 1.0f;

    uint32_t elapsed = nowMs - state_.fadeStartMs;
    uint32_t fadeMs = GetFadeMs();
    if (elapsed >= fadeMs) return 0.0f;
    return 1.0f - (float)elapsed / (float)fadeMs;
}

uint16_t SleepManager::GetPulseLevel(uint32_t nowMs) const
{
    uint32_t breatheMs = GetBreatheMs();
    uint32_t halfPeriod = breatheMs / 2U;
    uint32_t phase = nowMs % breatheMs;
    uint32_t ramp = phase < halfPeriod ? phase : (breatheMs - phase);
    uint32_t x = (ramp * PULSE_RESOLUTION) / halfPeriod;

    uint32_t smooth = (x * x * (3U * PULSE_RESOLUTION - 2U * x) +
                       (uint32_t)PULSE_RESOLUTION * PULSE_RESOLUTION / 2U) /
                      ((uint32_t)PULSE_RESOLUTION * PULSE_RESOLUTION);
    return (uint16_t)((smooth * smooth + PULSE_RESOLUTION / 2U) / PULSE_RESOLUTION);
}

uint8_t SleepManager::GetPulseRawBrightness(uint32_t nowMs) const
{
    uint16_t level = GetPulseLevel(nowMs);
    return (uint8_t)((level * STATUS_MAX_RAW_BRIGHTNESS + PULSE_RESOLUTION / 2U) / PULSE_RESOLUTION);
}

void SleepManager::ApplyLighting(HWKeyboard& kb, uint32_t nowMs)
{
    float scale = GetFadeScale(nowMs);
    if (scale >= 1.0f) return;

    // Skip status LEDs (82-84) — they are handled separately
    for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++) {
        if (i >= 82 && i < 85) continue;
        kb.ApplyStoredRgbByID(i, scale <= 0.0f ? 0.0f : scale);
    }
}

void SleepManager::RenderStatusLeds(HWKeyboard& kb, uint32_t nowMs,
                                     uint8_t statusLedStart, uint8_t statusLedCount)
{
    if (!state_.isSleeping) return;

    uint8_t brightness = GetPulseRawBrightness(nowMs);
    for (uint8_t i = 0; i < statusLedCount; i++) {
        if (brightness == 0)
            kb.SetRgbBufferByID(statusLedStart + i, {0, 0, 0});
        else
            kb.SetRgbBufferByID(statusLedStart + i,
                                HWKeyboard::Color_t{brightness, brightness, brightness},
                                1.0f, false);
    }
}
