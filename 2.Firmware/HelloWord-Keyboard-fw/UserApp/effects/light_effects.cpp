#include "light_effects.h"
#include "stm32f1xx_hal.h"

static uint8_t approxDist(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
    uint8_t dx = x1 > x2 ? x1 - x2 : x2 - x1;
    uint8_t dy = y1 > y2 ? y1 - y2 : y2 - y1;
    return dx > dy ? dx + ((dy * 3) >> 3) : dy + ((dx * 3) >> 3);
}

uint8_t Sin8(uint8_t theta)
{
    static const uint8_t lut[] = {
        0, 6, 13, 19, 25, 31, 37, 44, 50, 56, 62, 68, 74, 80, 86, 92,
        98,103,109,115,120,126,131,136,142,147,152,157,162,167,171,176,
        181,185,189,193,197,201,205,209,212,216,219,222,225,228,231,234,
        236,238,241,243,245,246,248,249,251,252,253,254,254,255,255,255
    };
    uint8_t idx = theta & 0x3F;
    if (theta & 0x40) idx = 63 - idx;
    uint8_t val = lut[idx];
    return (theta & 0x80) ? (128 - (val + 1) / 2) : (128 + val / 2);
}

HWKeyboard::Color_t HsvToRgb(uint8_t h, uint8_t s, uint8_t v)
{
    if (s == 0) return {v, v, v};

    uint8_t region = h / 43;
    uint8_t remainder = (h - region * 43) * 6;

    uint8_t p = ((uint16_t)v * (255 - s)) >> 8;
    uint8_t q = ((uint16_t)v * (255 - (((uint16_t)s * remainder) >> 8))) >> 8;
    uint8_t t = ((uint16_t)v * (255 - (((uint16_t)s * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
        case 0:  return {v, t, p};
        case 1:  return {q, v, p};
        case 2:  return {p, v, t};
        case 3:  return {p, q, v};
        case 4:  return {t, p, v};
        default: return {v, p, q};
    }
}

void GetLedPos(uint8_t idx, uint8_t& x, uint8_t& y)
{
    if (idx < 14)      { y = 0;  x = (13 - idx) * 17; }
    else if (idx < 29) { y = 16; x = (idx - 14) * 16; }
    else if (idx < 44) { y = 32; x = 6 + (43 - idx) * 16; }
    else if (idx < 58) { y = 48; x = 8 + (idx - 44) * 17; }
    else if (idx < 72) { y = 64; x = 12 + (71 - idx) * 16; }
    else if (idx < 82) {
        y = 80;
        static const uint8_t row5x[] = {0, 20, 40, 110, 170, 186, 202, 214, 226, 240};
        x = row5x[idx - 72];
    }
    else if (idx < 85) { y = 80; x = (uint8_t)(232 + (idx - 82) * 4); }
    else               { y = 96; x = (idx - 85) * 13; }
}

void RenderLightEffect(HWKeyboard& kb, const EffectColorConfig_t& colors)
{
    if (kb.brightnessLevel == 0) {
        for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
            kb.SetRgbBufferByID(i, {0, 0, 0});
        return;
    }

    uint32_t tick = HAL_GetTick();

    switch (kb.currentEffect) {
        case HWKeyboard::EFFECT_RAINBOW_SWEEP: {
            for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++) {
                uint8_t px, py;
                GetLedPos(i, px, py);
                uint8_t hue = (uint8_t)(px - tick / 8 + colors.rainbowHueOffset);
                uint8_t wave = Sin8((uint8_t)(tick / 5 + px / 2));
                uint8_t val = 178 + (uint8_t)(((uint16_t)wave * 77) >> 8);
                kb.SetRgbBufferByID(i, HsvToRgb(hue, 255, val));
            }
            break;
        }

        case HWKeyboard::EFFECT_REACTIVE: {
            static uint32_t lastDecay = 0;
            if (tick - lastDecay >= 3) {
                lastDecay = tick;
                for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
                    kb.keyBrightness[i] = Qsub8(kb.keyBrightness[i], 1);
            }
            for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++) {
                uint8_t b = kb.keyBrightness[i];
                if (b > 0) {
                    uint8_t hue = colors.reactiveH + (uint8_t)((255 - b) / 4);
                    uint8_t sat = b > 200 ? (uint8_t)(colors.reactiveS + (255 - b) / 2) : 255;
                    kb.SetRgbBufferByID(i, HsvToRgb(hue, sat, b));
                } else {
                    kb.SetRgbBufferByID(i, {0, 0, 0});
                }
            }
            break;
        }

        case HWKeyboard::EFFECT_AURORA: {
            for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++) {
                uint8_t px, py;
                GetLedPos(i, px, py);
                uint8_t w1 = Sin8((uint8_t)(tick / 11 + px / 2));
                uint8_t w2 = Sin8((uint8_t)(tick / 7 + (240 - px) / 2));
                uint8_t w3 = Sin8((uint8_t)(tick / 13 + px / 3 + py));
                uint8_t w4 = Sin8((uint8_t)(tick / 17 + (240 - px) / 3));

                uint8_t hue = colors.auroraTintH + (uint8_t)((w1 - 128 + w4 - 128) / 6);
                uint8_t val = (uint8_t)(((uint16_t)w2 + w3) >> 1);
                uint8_t sat = 255;
                if (val > 200) sat = Qsub8(255, (val - 200) * 4);

                HWKeyboard::Color_t c = HsvToRgb(hue, sat, val);
                uint8_t r = Qadd8(c.r, 2);
                uint8_t g = Qadd8((uint8_t)(((uint16_t)c.g * 200) >> 8), 5);
                uint8_t b = Qadd8((uint8_t)(((uint16_t)c.b * 145) >> 8), 7);
                kb.SetRgbBufferByID(i, {r, g, b});
            }
            break;
        }

        case HWKeyboard::EFFECT_RIPPLE: {
            static const uint8_t MAX_RIPPLES = 10;
            static const uint16_t RIPPLE_LIFE = 1000;
            static const uint8_t RING_WIDTH = 22;

            static struct { uint8_t x, y; uint16_t startTick; } ripples[MAX_RIPPLES];
            static uint8_t nextSlot = 0;
            static uint8_t prevPressed[11] = {0};
            static uint32_t lastDecay = 0;

            if (tick - lastDecay >= 3) {
                lastDecay = tick;
                for (uint8_t k = 0; k < HWKeyboard::KEY_NUMBER; k++)
                    kb.keyBrightness[k] = Qsub8(kb.keyBrightness[k], 10);
            }

            for (uint8_t k = 0; k < HWKeyboard::KEY_NUMBER; k++) {
                bool now = kb.keyBrightness[k] > 200;
                bool was = prevPressed[k >> 3] & (1 << (k & 7));
                if (now && !was) {
                    uint8_t px, py;
                    GetLedPos(k, px, py);
                    ripples[nextSlot % MAX_RIPPLES] = {px, py, (uint16_t)tick};
                    nextSlot++;
                }
                if (now) prevPressed[k >> 3] |= (1 << (k & 7));
                else     prevPressed[k >> 3] &= ~(1 << (k & 7));
            }

            for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++) {
                uint8_t px, py;
                GetLedPos(i, px, py);
                uint8_t bestBright = 0;
                uint8_t bestHue = 0;

                uint8_t count = nextSlot < MAX_RIPPLES ? nextSlot : MAX_RIPPLES;
                for (uint8_t r = 0; r < count; r++) {
                    uint16_t elapsed = (uint16_t)tick - ripples[r].startTick;
                    if (elapsed > RIPPLE_LIFE) continue;
                    uint8_t radius = (uint8_t)((elapsed * 65) >> 8);
                    uint8_t dist = approxDist(px, py, ripples[r].x, ripples[r].y);
                    int16_t ringDelta = (int16_t)dist - radius;
                    if (ringDelta < 0) ringDelta = -ringDelta;
                    if (ringDelta >= RING_WIDTH) continue;
                    uint8_t ring = (uint8_t)((RING_WIDTH - ringDelta) * (255 / RING_WIDTH));
                    uint8_t fade = 255 - (uint8_t)(((uint32_t)elapsed * 65) >> 8);
                    uint8_t bright = (uint8_t)(((uint16_t)ring * fade) >> 8);
                    if (bright > bestBright) {
                        bestBright = bright;
                        bestHue = (uint8_t)(ripples[r].x + ripples[r].y * 2 + elapsed / 6
                                            + colors.rippleH);
                    }
                }

                if (bestBright > 0)
                    kb.SetRgbBufferByID(i, HsvToRgb(bestHue, 255, bestBright));
                else
                    kb.SetRgbBufferByID(i, {0, 0, 0});
            }
            break;
        }

        case HWKeyboard::EFFECT_STATIC: {
            HWKeyboard::Color_t c = {colors.staticR, colors.staticG, colors.staticB};
            for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
                kb.SetRgbBufferByID(i, c);
            break;
        }

        default:
            break;
    }
}
