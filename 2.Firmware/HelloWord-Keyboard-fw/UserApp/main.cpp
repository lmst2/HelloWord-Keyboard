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


/* Light Effects -------------------------------------------------------------*/
static void RenderLightEffect()
{
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

        /* 2. Rainbow Sweep: arc of rainbow colors sweeping back and forth */
        case HWKeyboard::EFFECT_RAINBOW_SWEEP:
        {
            uint16_t period = HWKeyboard::LED_NUMBER * 2;
            uint16_t phase = (tick / 15) % period;
            int16_t center = phase < HWKeyboard::LED_NUMBER
                             ? (int16_t) phase
                             : (int16_t)(period - phase);

            for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
            {
                int16_t dist = (int16_t) i - center;
                int16_t absDist = dist < 0 ? -dist : dist;
                if (absDist < 25)
                {
                    uint8_t hue = (uint8_t)(dist * 5 + (tick / 8));
                    uint8_t val = 255 - (uint8_t)(absDist * 10);
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

        /* 7. Static: warm white */
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
        if (justPressed & 0x200) keyboard.SetEffect(HWKeyboard::EFFECT_STATIC);

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