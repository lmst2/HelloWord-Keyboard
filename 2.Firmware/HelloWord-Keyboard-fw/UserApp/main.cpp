#include <cstring>
#include "common_inc.h"
#include "configurations.h"
#include "HelloWord/hw_keyboard.h"


/* Component Definitions -----------------------------------------------------*/
KeyboardConfig_t config;
HWKeyboard keyboard(&hspi1);

static bool isSoftWareControlColor = false;
static bool isReceiveSuccess = false;


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


static void RenderLightEffect()
{
    uint32_t tick = HAL_GetTick();

    switch (keyboard.currentEffect)
    {
        case HWKeyboard::EFFECT_BREATHING:
        {
            uint32_t phase = (tick / 6) % 510;
            uint8_t val = phase < 255 ? phase : 510 - phase;
            for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
                keyboard.SetRgbBufferByID(i, {val, (uint8_t)(val / 5), (uint8_t)(val / 12)});
            break;
        }
        case HWKeyboard::EFFECT_STATIC:
        {
            for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
                keyboard.SetRgbBufferByID(i, {255, 180, 80});
            break;
        }
        case HWKeyboard::EFFECT_RAINBOW:
        {
            uint8_t hue = (tick / 20) % 256;
            HWKeyboard::Color_t c = HsvToRgb(hue, 255, 255);
            for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
                keyboard.SetRgbBufferByID(i, c);
            break;
        }
        case HWKeyboard::EFFECT_RAINBOW_WAVE:
        {
            for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
            {
                uint8_t hue = ((tick / 10) + (uint16_t) i * 3) % 256;
                keyboard.SetRgbBufferByID(i, HsvToRgb(hue, 255, 255));
            }
            break;
        }
        case HWKeyboard::EFFECT_SPECTRUM:
        {
            uint8_t hue = (tick / 30) % 256;
            uint32_t phase = (tick / 6) % 510;
            uint8_t val = phase < 255 ? phase : 510 - phase;
            HWKeyboard::Color_t c = HsvToRgb(hue, 255, val);
            for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
                keyboard.SetRgbBufferByID(i, c);
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

    static uint8_t prevFnCombo = 0;

    if (fnPressed)
    {
        uint8_t curFnCombo = 0;

        if (keyboard.KeyPressed(HWKeyboard::UP_ARROW))    curFnCombo |= 0x01;
        if (keyboard.KeyPressed(HWKeyboard::DOWN_ARROW))  curFnCombo |= 0x02;
        if (keyboard.KeyPressed(HWKeyboard::SPACE))        curFnCombo |= 0x04;
        if (keyboard.KeyPressed(HWKeyboard::NUM_1))        curFnCombo |= 0x08;
        if (keyboard.KeyPressed(HWKeyboard::NUM_2))        curFnCombo |= 0x10;
        if (keyboard.KeyPressed(HWKeyboard::NUM_3))        curFnCombo |= 0x20;
        if (keyboard.KeyPressed(HWKeyboard::NUM_4))        curFnCombo |= 0x40;
        if (keyboard.KeyPressed(HWKeyboard::NUM_5))        curFnCombo |= 0x80;

        uint8_t justPressed = curFnCombo & ~prevFnCombo;

        if (justPressed & 0x01) keyboard.IncreaseBrightness();
        if (justPressed & 0x02) keyboard.DecreaseBrightness();
        if (justPressed & 0x04) keyboard.NextEffect();
        if (justPressed & 0x08) keyboard.SetEffect(HWKeyboard::EFFECT_BREATHING);
        if (justPressed & 0x10) keyboard.SetEffect(HWKeyboard::EFFECT_STATIC);
        if (justPressed & 0x20) keyboard.SetEffect(HWKeyboard::EFFECT_RAINBOW);
        if (justPressed & 0x40) keyboard.SetEffect(HWKeyboard::EFFECT_RAINBOW_WAVE);
        if (justPressed & 0x80) keyboard.SetEffect(HWKeyboard::EFFECT_SPECTRUM);

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