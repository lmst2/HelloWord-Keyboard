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


/* Noise utilities for contour effect ----------------------------------------*/
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


static uint8_t noiseHash2D(uint8_t x, uint8_t y)
{
    return noisePerm(noisePerm(x) + y);
}


static uint8_t lerp8(uint8_t a, uint8_t b, uint8_t t)
{
    return (uint8_t)(((uint32_t) a * (256 - t) + (uint32_t) b * t) >> 8);
}


static uint8_t smoothNoise(uint16_t x, uint16_t y)
{
    uint8_t ix = x >> 8;
    uint8_t iy = y >> 8;
    uint8_t fx = x & 0xFF;
    uint8_t fy = y & 0xFF;

    uint16_t fx2 = ((uint16_t) fx * fx) >> 8;
    uint8_t sfx = (uint8_t)(3 * fx2 - 2 * ((fx2 * (uint16_t) fx) >> 8));
    uint16_t fy2 = ((uint16_t) fy * fy) >> 8;
    uint8_t sfy = (uint8_t)(3 * fy2 - 2 * ((fy2 * (uint16_t) fy) >> 8));

    uint8_t v00 = noiseHash2D(ix, iy);
    uint8_t v10 = noiseHash2D(ix + 1, iy);
    uint8_t v01 = noiseHash2D(ix, iy + 1);
    uint8_t v11 = noiseHash2D(ix + 1, iy + 1);

    return lerp8(lerp8(v00, v10, sfx), lerp8(v01, v11, sfx), sfy);
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

        /* 2. Rainbow Sweep: horizontal arc with subtle brightness wave */
        case HWKeyboard::EFFECT_RAINBOW_SWEEP:
        {
            uint16_t phase = (tick / 8) % 480;
            int16_t sweepX = phase < 240 ? (int16_t) phase : (int16_t)(480 - phase);

            for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
            {
                uint8_t px, py;
                getLedPos(i, px, py);

                int16_t dist = (int16_t) px - sweepX;
                int16_t absDist = dist < 0 ? -dist : dist;
                if (absDist < 60)
                {
                    uint8_t hue = (uint8_t)(dist * 3 + (tick / 10));
                    uint8_t baseBright = absDist < 50
                                         ? (uint8_t)(255 - absDist * 4)
                                         : (uint8_t)((60 - absDist) * 20);
                    uint8_t brightWave = sin8((uint8_t)(tick / 18 + px / 3));
                    uint8_t val = (uint8_t)(((uint16_t) baseBright * (215 + (brightWave >> 3))) >> 8);
                    keyboard.SetRgbBufferByID(i, HsvToRgb(hue, 255, val));
                } else
                {
                    keyboard.SetRgbBufferByID(i, {0, 0, 1});
                }
            }
            break;
        }

        /* 3. Starfall: white stars ignite and fade through blue/purple trail */
        case HWKeyboard::EFFECT_STARFALL:
        {
            static uint8_t state[HWKeyboard::LED_NUMBER] = {0};
            static uint32_t lastTick = 0;

            if (tick - lastTick >= 25)
            {
                lastTick = tick;
                if (fastRand() < 50)
                    state[fastRand() % HWKeyboard::LED_NUMBER] = 255;
                if (fastRand() < 50)
                    state[fastRand() % HWKeyboard::LED_NUMBER] = 255;

                for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
                    if (state[i] > 0 && state[i] < 255)
                        state[i] = qsub8(state[i], 4);
            }

            for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
            {
                uint8_t b = state[i];
                if (b == 255)
                {
                    keyboard.SetRgbBufferByID(i, {255, 255, 255});
                    state[i] = 254;
                } else if (b > 0)
                {
                    uint8_t r = b > 140 ? (uint8_t)((b - 140) * 2) : 0;
                    uint8_t g = b > 200 ? (uint8_t)((b - 200) / 2) : 0;
                    keyboard.SetRgbBufferByID(i, {r, g, b});
                } else
                {
                    keyboard.SetRgbBufferByID(i, {0, 0, 1});
                }
            }
            break;
        }

        /* 4. Reactive: keys flash on press and fade with cyan-to-blue trail */
        case HWKeyboard::EFFECT_REACTIVE:
        {
            static uint32_t lastDecay = 0;
            if (tick - lastDecay >= 5)
            {
                lastDecay = tick;
                for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
                    keyboard.keyBrightness[i] = qsub8(keyboard.keyBrightness[i], 3);
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
                uint8_t w1 = sin8((uint8_t)(tick / 11 + i * 7));
                uint8_t w2 = sin8((uint8_t)(tick / 7 + (255 - i) * 13));
                uint8_t w3 = sin8((uint8_t)(tick / 13 + i * 5));
                uint8_t w4 = sin8((uint8_t)(tick / 17 + (255 - i) * 3));

                uint8_t hue = 110 + (uint8_t)((w1 - 128 + w4 - 128) / 6);
                uint8_t val = (uint8_t)(((uint16_t) w2 + w3) >> 1);

                uint8_t sat = 255;
                if (val > 200)
                {
                    uint8_t boost = (val - 200) * 4;
                    sat = qsub8(255, boost);
                }

                uint8_t r, g, b;
                HWKeyboard::Color_t c = HsvToRgb(hue, sat, val);
                r = c.r;
                g = (uint8_t)(((uint16_t) c.g * 200) >> 8);
                b = (uint8_t)(((uint16_t) c.b * 145) >> 8);
                r = qadd8(r, 2);
                g = qadd8(g, 5);
                b = qadd8(b, 7);

                keyboard.SetRgbBufferByID(i, {r, g, b});
            }
            break;
        }

        /* 6. Digital Rain: Matrix-style green rain drops */
        case HWKeyboard::EFFECT_DIGITAL_RAIN:
        {
            static uint8_t rain[HWKeyboard::LED_NUMBER] = {0};
            static uint32_t lastRainTick = 0;

            if (tick - lastRainTick >= 30)
            {
                lastRainTick = tick;

                if (fastRand() < 60)
                    rain[fastRand() % HWKeyboard::LED_NUMBER] = 255;

                for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
                {
                    if (rain[i] > 0 && rain[i] < 255)
                        rain[i] = qsub8(rain[i], 5);
                }
            }

            for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
            {
                uint8_t v = rain[i];
                if (v == 255)
                {
                    keyboard.SetRgbBufferByID(i, {180, 255, 180});
                    rain[i] = 254;
                } else if (v > 0)
                {
                    uint8_t wb = v > 200 ? (uint8_t)((v - 200) * 3) : 0;
                    keyboard.SetRgbBufferByID(i, {wb, v, (uint8_t)(v / 6)});
                } else
                {
                    keyboard.SetRgbBufferByID(i, {0, 0, 1});
                }
            }
            break;
        }

        /* 7. Contour: topographic contour lines flowing with Perlin noise */
        case HWKeyboard::EFFECT_CONTOUR:
        {
            for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
            {
                uint8_t px, py;
                getLedPos(i, px, py);

                uint16_t nx = (uint16_t) px * 5 + (uint16_t)(tick / 30);
                uint16_t ny = (uint16_t) py * 8 + (uint16_t)(tick / 80);
                uint8_t n1 = smoothNoise(nx, ny);

                uint16_t nx2 = (uint16_t) px * 10 + (uint16_t)(tick / 50) + 8000;
                uint16_t ny2 = (uint16_t) py * 16 + (uint16_t)(tick / 60) + 12000;
                uint8_t n2 = smoothNoise(nx2, ny2);

                uint8_t combined = (uint8_t)(((uint16_t) n1 * 3 + n2) >> 2);

                uint8_t hue = combined;
                uint8_t contourWave = sin8(combined * 2);

                uint8_t val = 100 + (contourWave >> 2);
                keyboard.SetRgbBufferByID(i, HsvToRgb(hue, 255, val));
            }
            break;
        }

        /* 8. Ripple: rings expand outward from pressed keys */
        case HWKeyboard::EFFECT_RIPPLE:
        {
            static const uint8_t MAX_RIPPLES = 10;
            static const uint16_t RIPPLE_LIFE = 800;
            static const uint8_t RING_WIDTH = 18;

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
                uint8_t bestHue = 140;

                uint8_t count = nextSlot < MAX_RIPPLES ? nextSlot : MAX_RIPPLES;
                for (uint8_t r = 0; r < count; r++)
                {
                    uint16_t elapsed = (uint16_t) tick - ripples[r].startTick;
                    if (elapsed > RIPPLE_LIFE) continue;

                    uint8_t radius = (uint8_t)((elapsed * 77) >> 8);
                    uint8_t dist = approxDist(px, py, ripples[r].x, ripples[r].y);
                    int16_t ringDelta = (int16_t) dist - radius;
                    if (ringDelta < 0) ringDelta = -ringDelta;
                    if (ringDelta >= RING_WIDTH) continue;

                    uint8_t ring = (uint8_t)((RING_WIDTH - ringDelta) * (255 / RING_WIDTH));
                    uint8_t fade = 255 - (uint8_t)((elapsed * 81) >> 8);
                    uint8_t bright = (uint8_t)(((uint16_t) ring * fade) >> 8);

                    if (bright > bestBright)
                    {
                        bestBright = bright;
                        bestHue = 140 + (uint8_t)(elapsed / 8);
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
        if (justPressed & 0x020) keyboard.SetEffect(HWKeyboard::EFFECT_STARFALL);
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