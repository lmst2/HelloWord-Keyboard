#include <cstring>
#include "common_inc.h"
#include "configurations.h"
#include "HelloWord/hw_keyboard.h"


/* Component Definitions -----------------------------------------------------*/
KeyboardConfig_t config;
HWKeyboard keyboard(&hspi1);

static bool isSoftWareControlColor = false;
static bool isReceiveSuccess = false;


/* Utility Functions ---------------------------------------------------------*/
static uint8_t sin8(uint8_t theta)
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


static uint16_t prngState = 0;

static uint8_t fastRand()
{
    if (prngState == 0) prngState = (uint16_t) HAL_GetTick() | 1;
    prngState ^= prngState << 7;
    prngState ^= prngState >> 9;
    prngState ^= prngState << 8;
    return (uint8_t)(prngState >> 8);
}


static HWKeyboard::Color_t HsvToRgb(uint8_t h, uint8_t s, uint8_t v)
{
    if (s == 0) return {v, v, v};

    uint8_t region = h / 43;
    uint8_t remainder = (h - region * 43) * 6;

    uint8_t p = ((uint16_t) v * (255 - s)) >> 8;
    uint8_t q = ((uint16_t) v * (255 - (((uint16_t) s * remainder) >> 8))) >> 8;
    uint8_t t = ((uint16_t) v * (255 - (((uint16_t) s * (255 - remainder)) >> 8))) >> 8;

    switch (region)
    {
        case 0:  return {v, t, p};
        case 1:  return {q, v, p};
        case 2:  return {p, v, t};
        case 3:  return {p, q, v};
        case 4:  return {t, p, v};
        default: return {v, p, q};
    }
}


static inline uint8_t qadd8(uint8_t a, uint8_t b)
{
    uint16_t sum = (uint16_t) a + b;
    return sum > 255 ? 255 : (uint8_t) sum;
}


static inline uint8_t qsub8(uint8_t a, uint8_t b)
{
    return a > b ? a - b : 0;
}


/* 3D noise for contour effect -----------------------------------------------*/
static uint8_t noisePerm(uint8_t x)
{
    static const uint8_t p[] = {
        151,160,137, 91, 90, 15,131, 13,201, 95, 96, 53,194,233,  7,225,
        140, 36,103, 30, 69,142,  8, 99, 37,240, 21, 10, 23,190,  6,148,
        247,120,234, 75,  0, 26,197, 62, 94,252,219,203,117, 35, 11, 32,
         65,195, 76,204, 98, 57,227,186,132, 83,158, 87,144, 41, 82,167
    };
    return p[x & 0x3F];
}


static uint8_t lerp8(uint8_t a, uint8_t b, uint8_t t)
{
    return (uint8_t)(((uint32_t) a * (256 - t) + (uint32_t) b * t) >> 8);
}


static uint8_t valueNoise3D(uint16_t x, uint16_t y, uint16_t z)
{
    uint8_t ix = x >> 8, iy = y >> 8, iz = z >> 8;
    uint8_t fx = x & 0xFF, fy = y & 0xFF, fz = z & 0xFF;

    uint16_t fx2 = ((uint16_t) fx * fx) >> 8;
    uint8_t sfx = (uint8_t)(3 * fx2 - 2 * ((fx2 * (uint16_t) fx) >> 8));
    uint16_t fy2 = ((uint16_t) fy * fy) >> 8;
    uint8_t sfy = (uint8_t)(3 * fy2 - 2 * ((fy2 * (uint16_t) fy) >> 8));
    uint16_t fz2 = ((uint16_t) fz * fz) >> 8;
    uint8_t sfz = (uint8_t)(3 * fz2 - 2 * ((fz2 * (uint16_t) fz) >> 8));

    #define H3(a,b,c) noisePerm(noisePerm(noisePerm(a) + (b)) + (c))

    uint8_t n00 = lerp8(H3(ix, iy, iz),     H3(ix+1, iy, iz),     sfx);
    uint8_t n10 = lerp8(H3(ix, iy+1, iz),   H3(ix+1, iy+1, iz),   sfx);
    uint8_t n01 = lerp8(H3(ix, iy, iz+1),   H3(ix+1, iy, iz+1),   sfx);
    uint8_t n11 = lerp8(H3(ix, iy+1, iz+1), H3(ix+1, iy+1, iz+1), sfx);

    #undef H3

    return lerp8(lerp8(n00, n10, sfy), lerp8(n01, n11, sfy), sfz);
}


/* Ripple utilities ----------------------------------------------------------*/
static void getLedPos(uint8_t idx, uint8_t& x, uint8_t& y)
{
    // WS2812B chain snakes: rows 0,2,4 are wired right-to-left
    if (idx < 14)      { y = 0;  x = (13 - idx) * 17; }
    else if (idx < 29) { y = 16; x = (idx - 14) * 16; }
    else if (idx < 44) { y = 32; x = 6 + (43 - idx) * 16; }
    else if (idx < 58) { y = 48; x = 8 + (idx - 44) * 17; }
    else if (idx < 72) { y = 64; x = 12 + (71 - idx) * 16; }
    else if (idx < 82)
    {
        y = 80;
        static const uint8_t row5x[] = {0, 20, 40, 110, 170, 186, 202, 214, 226, 240};
        x = row5x[idx - 72];
    }
    else if (idx < 85) { y = 80; x = (uint8_t)(232 + (idx - 82) * 4); }
    else               { y = 96; x = (idx - 85) * 13; }
}


static uint8_t approxDist(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
    uint8_t dx = x1 > x2 ? x1 - x2 : x2 - x1;
    uint8_t dy = y1 > y2 ? y1 - y2 : y2 - y1;
    return dx > dy ? dx + ((dy * 3) >> 3) : dy + ((dx * 3) >> 3);
}


/* Light Effects -------------------------------------------------------------*/
static void RenderLightEffect()
{
    if (keyboard.brightnessLevel == 0)
    {
        for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
            keyboard.SetRgbBufferByID(i, {0, 0, 0});
        return;
    }

    uint32_t tick = HAL_GetTick();

    switch (keyboard.currentEffect)
    {
        /* 1. Breathing: warm amber pulse */
        case HWKeyboard::EFFECT_BREATHING:
        {
            uint32_t phase = (tick / 6) % 510;
            uint8_t val = phase < 255 ? (uint8_t) phase : (uint8_t)(510 - phase);
            for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
                keyboard.SetRgbBufferByID(i, {val, (uint8_t)(val / 5), (uint8_t)(val / 12)});
            break;
        }

        /* 2. Rainbow Sweep: full-keyboard rainbow with faster brightness wave */
        case HWKeyboard::EFFECT_RAINBOW_SWEEP:
        {
            for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
            {
                uint8_t px, py;
                getLedPos(i, px, py);

                uint8_t hue = (uint8_t)(px - tick / 8);
                uint8_t wave = sin8((uint8_t)(tick / 5 + px / 2));
                uint8_t val = 178 + (uint8_t)(((uint16_t) wave * 77) >> 8);

                keyboard.SetRgbBufferByID(i, HsvToRgb(hue, 255, val));
            }
            break;
        }

        /* 3. Flame: noise-driven fire rising from bottom */
        case HWKeyboard::EFFECT_FLAME:
        {
            static HWKeyboard::Color_t buf[HWKeyboard::LED_NUMBER];
            uint16_t nyBase = -(uint16_t)(tick / 2);
            uint16_t nz = (uint16_t)(tick / 9);

            for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
            {
                uint8_t px, py;
                getLedPos(i, px, py);

                uint8_t n = valueNoise3D((uint16_t)px * 2, (uint16_t)py * 3 + nyBase, nz);

                int16_t h = (int16_t)n + (int16_t)n / 2 + (int16_t)py * 2 - 180;
                if (h < 0) h = 0;
                if (h > 255) h = 255;

                uint8_t heat = (uint8_t)h;
                if (heat < 85)       buf[i] = {(uint8_t)(heat * 3), 0, 0};
                else if (heat < 170) buf[i] = {255, (uint8_t)((heat - 85) * 3), 0};
                else                 buf[i] = {255, 255, (uint8_t)((heat - 170) * 3)};
            }

            for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
                keyboard.SetRgbBufferByID(i, buf[i]);
            break;
        }

        /* 4. Reactive: keys flash on press and fade (~0.8s) */
        case HWKeyboard::EFFECT_REACTIVE:
        {
            static uint32_t lastDecay = 0;
            if (tick - lastDecay >= 3)
            {
                lastDecay = tick;
                for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
                    keyboard.keyBrightness[i] = qsub8(keyboard.keyBrightness[i], 1);
            }

            for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
            {
                uint8_t b = keyboard.keyBrightness[i];
                if (b > 0)
                {
                    uint8_t hue = 128 + (uint8_t)((255 - b) / 4);
                    uint8_t sat = b > 200 ? (uint8_t)(200 + (255 - b) / 2) : 255;
                    keyboard.SetRgbBufferByID(i, HsvToRgb(hue, sat, b));
                } else
                {
                    keyboard.SetRgbBufferByID(i, {0, 0, 1});
                }
            }
            break;
        }

        /* 5. Aurora: multi-layered flowing ocean waves (Pacifica-inspired) */
        case HWKeyboard::EFFECT_AURORA:
        {
            for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
            {
                uint8_t px, py;
                getLedPos(i, px, py);

                uint8_t w1 = sin8((uint8_t)(tick / 11 + px / 2));
                uint8_t w2 = sin8((uint8_t)(tick / 7 + (240 - px) / 2));
                uint8_t w3 = sin8((uint8_t)(tick / 13 + px / 3 + py));
                uint8_t w4 = sin8((uint8_t)(tick / 17 + (240 - px) / 3));

                uint8_t hue = 110 + (uint8_t)((w1 - 128 + w4 - 128) / 6);
                uint8_t val = (uint8_t)(((uint16_t) w2 + w3) >> 1);

                uint8_t sat = 255;
                if (val > 200)
                    sat = qsub8(255, (val - 200) * 4);

                HWKeyboard::Color_t c = HsvToRgb(hue, sat, val);
                uint8_t r = qadd8(c.r, 2);
                uint8_t g = qadd8((uint8_t)(((uint16_t) c.g * 200) >> 8), 5);
                uint8_t b = qadd8((uint8_t)(((uint16_t) c.b * 145) >> 8), 7);
                keyboard.SetRgbBufferByID(i, {r, g, b});
            }
            break;
        }

        /* 6. Digital Rain: drops fall top-to-bottom, one LED per row */
        case HWKeyboard::EFFECT_DIGITAL_RAIN:
        {
            static const uint8_t MAX_DROPS = 8;
            static struct { uint8_t x; int16_t y; uint8_t speed; } drops[MAX_DROPS] = {};
            static uint8_t dropActive = 0;
            static uint32_t lastRainTick = 0;

            if (tick - lastRainTick >= 30)
            {
                lastRainTick = tick;
                for (uint8_t d = 0; d < MAX_DROPS; d++)
                {
                    if (dropActive & (1 << d))
                    {
                        drops[d].y += drops[d].speed;
                        if (drops[d].y > 150) dropActive &= ~(1 << d);
                    }
                }
                if (fastRand() < 60)
                {
                    for (uint8_t d = 0; d < MAX_DROPS; d++)
                    {
                        if (!(dropActive & (1 << d)))
                        {
                            uint8_t refLed = fastRand() % 15;
                            uint8_t rx, ry;
                            getLedPos(14 + refLed, rx, ry);
                            drops[d].x = rx;
                            drops[d].y = 0;
                            drops[d].speed = 3 + (fastRand() & 1);
                            dropActive |= (1 << d);
                            break;
                        }
                    }
                }
            }

            for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
            {
                uint8_t px, py;
                getLedPos(i, px, py);
                uint8_t bestGreen = 0;
                uint8_t bestWhite = 0;

                for (uint8_t d = 0; d < MAX_DROPS; d++)
                {
                    if (!(dropActive & (1 << d))) continue;
                    uint8_t dx = px > drops[d].x ? px - drops[d].x : drops[d].x - px;
                    if (dx > 8) continue;

                    int16_t dy = drops[d].y - (int16_t) py;
                    if (dy < -2 || dy > 80) continue;

                    uint8_t green;
                    if (dy < 0) { green = 0; }
                    else if (dy < 4)
                    {
                        green = 255;
                        uint8_t w = (uint8_t)((4 - dy) * 50);
                        if (w > bestWhite) bestWhite = w;
                    } else
                    {
                        green = qsub8(255, (uint8_t)((dy - 4) * 3));
                    }
                    if (green > bestGreen) bestGreen = green;
                }

                if (bestGreen > 0 || bestWhite > 0)
                    keyboard.SetRgbBufferByID(i, {bestWhite, qadd8(bestGreen, bestWhite), (uint8_t)(bestGreen / 8)});
                else
                    keyboard.SetRgbBufferByID(i, {0, 0, 1});
            }
            break;
        }

        /* 7. Contour: 3D value noise field, time as z-dimension */
        case HWKeyboard::EFFECT_CONTOUR:
        {
            uint16_t tz = (uint16_t)(tick / 78);

            for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
            {
                uint8_t px, py;
                getLedPos(i, px, py);

                uint8_t wx = sin8((uint8_t)(py / 2 + (uint8_t)(tick / 70))) >> 3;
                uint8_t wy = sin8((uint8_t)(px / 3 + (uint8_t)(tick / 90))) >> 3;

                uint16_t nx = (uint16_t)(px + wx) * 3;
                uint16_t ny = (uint16_t)(py + wy) * 5;

                uint8_t n = valueNoise3D(nx, ny, tz);
                keyboard.SetRgbBufferByID(i, HsvToRgb(n, 240, 200));
            }
            break;
        }

        /* 8. Ripple: colorful rings expand from pressed keys */
        case HWKeyboard::EFFECT_RIPPLE:
        {
            static const uint8_t MAX_RIPPLES = 10;
            static const uint16_t RIPPLE_LIFE = 1000;
            static const uint8_t RING_WIDTH = 22;

            static struct { uint8_t x, y; uint16_t startTick; } ripples[MAX_RIPPLES];
            static uint8_t nextSlot = 0;
            static uint8_t prevPressed[11] = {0};
            static uint32_t lastDecay = 0;

            if (tick - lastDecay >= 3)
            {
                lastDecay = tick;
                for (uint8_t k = 0; k < HWKeyboard::KEY_NUMBER; k++)
                    keyboard.keyBrightness[k] = qsub8(keyboard.keyBrightness[k], 10);
            }

            for (uint8_t k = 0; k < HWKeyboard::KEY_NUMBER; k++)
            {
                bool now = keyboard.keyBrightness[k] > 200;
                bool was = prevPressed[k >> 3] & (1 << (k & 7));
                if (now && !was)
                {
                    uint8_t px, py;
                    getLedPos(k, px, py);
                    ripples[nextSlot % MAX_RIPPLES] = {px, py, (uint16_t) tick};
                    nextSlot++;
                }
                if (now) prevPressed[k >> 3] |= (1 << (k & 7));
                else     prevPressed[k >> 3] &= ~(1 << (k & 7));
            }

            for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
            {
                uint8_t px, py;
                getLedPos(i, px, py);

                uint8_t bestBright = 0;
                uint8_t bestHue = 0;

                uint8_t count = nextSlot < MAX_RIPPLES ? nextSlot : MAX_RIPPLES;
                for (uint8_t r = 0; r < count; r++)
                {
                    uint16_t elapsed = (uint16_t) tick - ripples[r].startTick;
                    if (elapsed > RIPPLE_LIFE) continue;

                    uint8_t radius = (uint8_t)((elapsed * 65) >> 8);
                    uint8_t dist = approxDist(px, py, ripples[r].x, ripples[r].y);
                    int16_t ringDelta = (int16_t) dist - radius;
                    if (ringDelta < 0) ringDelta = -ringDelta;
                    if (ringDelta >= RING_WIDTH) continue;

                    uint8_t ring = (uint8_t)((RING_WIDTH - ringDelta) * (255 / RING_WIDTH));
                    uint8_t fade = 255 - (uint8_t)(((uint32_t) elapsed * 65) >> 8);
                    uint8_t bright = (uint8_t)(((uint16_t) ring * fade) >> 8);

                    if (bright > bestBright)
                    {
                        bestBright = bright;
                        bestHue = (uint8_t)(ripples[r].x + ripples[r].y * 2 + elapsed / 6);
                    }
                }

                if (bestBright > 0)
                    keyboard.SetRgbBufferByID(i, HsvToRgb(bestHue, 255, bestBright));
                else
                    keyboard.SetRgbBufferByID(i, {0, 0, 1});
            }
            break;
        }

        /* 9. Static: warm white */
        case HWKeyboard::EFFECT_STATIC:
        {
            for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
                keyboard.SetRgbBufferByID(i, {255, 180, 80});
            break;
        }

        default:
            break;
    }
}


/* Status Indicator LEDs (82-84, near arrow keys) ---------------------------*/
static const uint8_t STATUS_LED_START = 82;
static const uint8_t STATUS_LED_COUNT = 3;

static void UpdateStatusLEDs()
{
    HWKeyboard::Color_t c = keyboard.isCapsLocked
                            ? HWKeyboard::Color_t{255, 255, 255}
                            : HWKeyboard::Color_t{0, 0, 0};
    for (uint8_t i = 0; i < STATUS_LED_COUNT; i++)
        keyboard.SetRgbBufferByID(STATUS_LED_START + i, c);
}


/* Main Entry ----------------------------------------------------------------*/
void Main()
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
    PWR->CR |= PWR_CR_DBP;
    BKP->DR2 = 0;

    IWDG->KR  = 0x5555;
    IWDG->PR  = 4;
    IWDG->RLR = 0xFFF;
    IWDG->KR  = 0xCCCC;

    EEPROM eeprom;
    eeprom.Pull(0, config);
    if (config.configStatus != CONFIG_OK)
    {
        config = KeyboardConfig_t{
            .configStatus = CONFIG_OK,
            .serialNum=123,
            .keyMap={}
        };
        memset(config.keyMap, -1, 128);
        eeprom.Push(0, config);
    }

    HAL_TIM_Base_Start_IT(&htim4);

    while (true)
    {
        if (isReceiveSuccess)
        {
            keyboard.SyncLights();
            isReceiveSuccess = false;
        }

        if (!isSoftWareControlColor)
        {
            RenderLightEffect();
            UpdateStatusLEDs();
            keyboard.SyncLights();
        }

        IWDG->KR = 0xAAAA;
    }
}


/* Event Callbacks -----------------------------------------------------------*/
extern "C" void OnTimerCallback() // 1000Hz callback
{
    keyboard.ScanKeyStates();
    keyboard.ApplyDebounceFilter(100);
    keyboard.ApplyKeyDebounce(8);

    bool fnPressed = keyboard.FnPressed();
    keyboard.Remap(1);
    keyboard.UpdateKeyPressState();

    static uint16_t prevFnCombo = 0;

    if (fnPressed)
    {
        uint16_t curFnCombo = 0;

        if (keyboard.KeyPressed(HWKeyboard::UP_ARROW))    curFnCombo |= 0x001;
        if (keyboard.KeyPressed(HWKeyboard::DOWN_ARROW))  curFnCombo |= 0x002;
        if (keyboard.KeyPressed(HWKeyboard::SPACE))        curFnCombo |= 0x004;
        if (keyboard.KeyPressed(HWKeyboard::NUM_1))        curFnCombo |= 0x008;
        if (keyboard.KeyPressed(HWKeyboard::NUM_2))        curFnCombo |= 0x010;
        if (keyboard.KeyPressed(HWKeyboard::NUM_3))        curFnCombo |= 0x020;
        if (keyboard.KeyPressed(HWKeyboard::NUM_4))        curFnCombo |= 0x040;
        if (keyboard.KeyPressed(HWKeyboard::NUM_5))        curFnCombo |= 0x080;
        if (keyboard.KeyPressed(HWKeyboard::NUM_6))        curFnCombo |= 0x100;
        if (keyboard.KeyPressed(HWKeyboard::NUM_7))        curFnCombo |= 0x200;
        if (keyboard.KeyPressed(HWKeyboard::NUM_8))        curFnCombo |= 0x400;
        if (keyboard.KeyPressed(HWKeyboard::NUM_9))        curFnCombo |= 0x800;

        uint16_t justPressed = curFnCombo & ~prevFnCombo;

        if (justPressed & 0x001) keyboard.IncreaseBrightness();
        if (justPressed & 0x002) keyboard.DecreaseBrightness();
        if (justPressed & 0x004) keyboard.NextEffect();
        if (justPressed & 0x008) keyboard.SetEffect(HWKeyboard::EFFECT_BREATHING);
        if (justPressed & 0x010) keyboard.SetEffect(HWKeyboard::EFFECT_RAINBOW_SWEEP);
        if (justPressed & 0x020) keyboard.SetEffect(HWKeyboard::EFFECT_FLAME);
        if (justPressed & 0x040) keyboard.SetEffect(HWKeyboard::EFFECT_REACTIVE);
        if (justPressed & 0x080) keyboard.SetEffect(HWKeyboard::EFFECT_AURORA);
        if (justPressed & 0x100) keyboard.SetEffect(HWKeyboard::EFFECT_DIGITAL_RAIN);
        if (justPressed & 0x200) keyboard.SetEffect(HWKeyboard::EFFECT_CONTOUR);
        if (justPressed & 0x400) keyboard.SetEffect(HWKeyboard::EFFECT_RIPPLE);
        if (justPressed & 0x800) keyboard.SetEffect(HWKeyboard::EFFECT_STATIC);

        prevFnCombo = curFnCombo;

        uint8_t* report = keyboard.GetHidReportBuffer(1);
        memset(report + 1, 0, HWKeyboard::KEY_REPORT_SIZE - 1);
    }
    else
    {
        prevFnCombo = 0;
    }

    USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS,
                               keyboard.GetHidReportBuffer(1),
                               HWKeyboard::KEY_REPORT_SIZE);
}


extern "C"
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef* hspi)
{
    keyboard.isRgbTxBusy = false;
}

static void RebootToDfu()
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
    PWR->CR |= PWR_CR_DBP;
    BKP->DR1 = 0xB011U;
    NVIC_SystemReset();
}

extern "C"
void HID_RxCpltCallback(uint8_t* _data)
{
    if (_data[0] == 1)
    {
        keyboard.isCapsLocked = _data[1] & 0x02;
        return;
    }

    if(_data[1] == 0xdf)  RebootToDfu();
    if(_data[1] == 0xbd)  isSoftWareControlColor= false;
    if(_data[1] == 0xac) {
        isSoftWareControlColor = true;
        uint8_t pageIndex = _data[2];
        for (uint8_t i = 0; i < 10; i++) {
            if(i+pageIndex*10>=HWKeyboard::LED_NUMBER) {
                isReceiveSuccess = true;
                break;
            }
            keyboard.SetRgbBufferByID(i+pageIndex*10,
                                      HWKeyboard::Color_t{_data[3+i*3], _data[4+i*3], _data[5+i*3]});
        }
    }
}