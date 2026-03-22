#ifndef LIGHT_EFFECTS_H
#define LIGHT_EFFECTS_H

#include <stdint.h>
#include "HelloWord/hw_keyboard.h"
#include "configurations.h"

void RenderLightEffect(HWKeyboard& kb, const EffectColorConfig_t& colors);

// LED physical position lookup (used by effects and ripple)
void GetLedPos(uint8_t idx, uint8_t& x, uint8_t& y);

// Color utilities
HWKeyboard::Color_t HsvToRgb(uint8_t h, uint8_t s, uint8_t v);
uint8_t Sin8(uint8_t theta);

static inline uint8_t Qadd8(uint8_t a, uint8_t b) {
    uint16_t sum = (uint16_t)a + b;
    return sum > 255 ? 255 : (uint8_t)sum;
}

static inline uint8_t Qsub8(uint8_t a, uint8_t b) {
    return a > b ? a - b : 0;
}

#endif // LIGHT_EFFECTS_H
