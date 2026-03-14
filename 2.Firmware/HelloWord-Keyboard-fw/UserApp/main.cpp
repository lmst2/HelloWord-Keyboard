#include <cstring>
#include "common_inc.h"
#include "configurations.h"
#include "HelloWord/hw_keyboard.h"


/* Component Definitions -----------------------------------------------------*/
KeyboardConfig_t config;
HWKeyboard keyboard(&hspi1);

static bool isSoftWareControlColor = false;
static bool isReceiveSuccess = false;


enum TouchBarMode_t : uint8_t
{
    TOUCHBAR_MODE_PAN = 0,
    TOUCHBAR_MODE_APP_SWITCH,
    TOUCHBAR_MODE_DESKTOP_SWITCH,
    TOUCHBAR_MODE_COUNT
};

struct TouchBarSession_t
{
    TouchBarMode_t mode = TOUCHBAR_MODE_PAN;
    bool isTouching = false;
    bool isGestureActive = false;
    bool isDesktopSeekMode = false;
    uint32_t touchStartMs = 0;
    uint32_t lastPanMs = 0;
    uint32_t lastStepMs = 0;
    int16_t anchorPosition = 0;
    int16_t currentPosition = 0;
    int16_t emittedSteps = 0;
};

struct SyntheticKeyState_t
{
    bool holdLeftAlt = false;
    uint8_t leftShiftFrames = 0;
    uint8_t leftCtrlFrames = 0;
    uint8_t leftGuiFrames = 0;
    uint8_t tabFrames = 0;
    uint8_t leftArrowFrames = 0;
    uint8_t rightArrowFrames = 0;
};

struct StatusBlinkState_t
{
    bool active = false;
    uint8_t flashCount = 0;
    uint32_t startMs = 0;
};

static TouchBarSession_t touchBarSession;
static SyntheticKeyState_t syntheticKeys;
static StatusBlinkState_t statusBlink;
static uint8_t lastKeyboardReport[HWKeyboard::KEY_REPORT_SIZE]{};
static uint8_t pendingMouseReport[HWKeyboard::MOUSE_REPORT_SIZE]{};
static bool hasKeyboardReportSnapshot = false;
static bool hasPendingMouseReport = false;

static const uint32_t TOUCHBAR_ACTIVATION_MS = 100;
static const uint32_t TOUCHBAR_DESKTOP_HOLD_MS = 1000;
static const uint32_t TOUCHBAR_PAN_INTERVAL_MS = 12;
static const uint32_t TOUCHBAR_STEP_INTERVAL_MS = 55;
static const uint32_t STATUS_BLINK_PHASE_MS = 120;
static const int16_t TOUCHBAR_POSITION_SCALE = 256;
static const int16_t TOUCHBAR_DESKTOP_SWIPE_DISTANCE = 192;
static const int16_t TOUCHBAR_PAN_DEADZONE = 64;
static const int16_t TOUCHBAR_STEP_DISTANCE = 160;
static const uint8_t SYNTHETIC_PULSE_FRAMES = 2;


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


static inline int16_t abs16(int16_t value)
{
    return value >= 0 ? value : (int16_t) -value;
}


static int16_t GetTouchBarPosition(uint8_t touchState)
{
    uint16_t weightedSum = 0;
    uint8_t activeCount = 0;

    for (uint8_t i = 0; i < HWKeyboard::TOUCHPAD_NUMBER; i++)
    {
        const uint8_t bit = (uint8_t) (1U << (HWKeyboard::TOUCHPAD_NUMBER - 1U - i));
        if (touchState & bit)
        {
            weightedSum += (uint16_t) i * TOUCHBAR_POSITION_SCALE;
            activeCount++;
        }
    }

    if (activeCount == 0)
        return -1;

    return (int16_t) (weightedSum / activeCount);
}


static void TriggerStatusBlink(uint8_t flashCount)
{
    statusBlink.active = flashCount > 0;
    statusBlink.flashCount = flashCount;
    statusBlink.startMs = HAL_GetTick();
}


static void ClearTouchBarActions()
{
    touchBarSession.isTouching = false;
    touchBarSession.isGestureActive = false;
    touchBarSession.isDesktopSeekMode = false;
    touchBarSession.touchStartMs = 0;
    touchBarSession.lastPanMs = 0;
    touchBarSession.lastStepMs = 0;
    touchBarSession.anchorPosition = 0;
    touchBarSession.currentPosition = 0;
    touchBarSession.emittedSteps = 0;
    syntheticKeys.holdLeftAlt = false;
}


static void CycleTouchBarMode()
{
    touchBarSession.mode = (TouchBarMode_t) ((touchBarSession.mode + 1) % TOUCHBAR_MODE_COUNT);
    TriggerStatusBlink((uint8_t) touchBarSession.mode + 1);
}


static void QueueMousePan(int8_t pan)
{
    if (pan == 0)
        return;

    keyboard.SetMousePan(pan);
    memcpy(pendingMouseReport, keyboard.GetHidReportBuffer(3), HWKeyboard::MOUSE_REPORT_SIZE);
    hasPendingMouseReport = true;
    keyboard.ClearMouseReport();
}


static void ApplySyntheticKeys()
{
    if (syntheticKeys.holdLeftAlt)
        keyboard.Press(HWKeyboard::LEFT_ALT);

    if (syntheticKeys.leftShiftFrames > 0)
    {
        keyboard.Press(HWKeyboard::LEFT_SHIFT);
        syntheticKeys.leftShiftFrames--;
    }
    if (syntheticKeys.leftCtrlFrames > 0)
    {
        keyboard.Press(HWKeyboard::LEFT_CTRL);
        syntheticKeys.leftCtrlFrames--;
    }
    if (syntheticKeys.leftGuiFrames > 0)
    {
        keyboard.Press(HWKeyboard::LEFT_GUI);
        syntheticKeys.leftGuiFrames--;
    }
    if (syntheticKeys.tabFrames > 0)
    {
        keyboard.Press(HWKeyboard::TAB);
        syntheticKeys.tabFrames--;
    }
    if (syntheticKeys.leftArrowFrames > 0)
    {
        keyboard.Press(HWKeyboard::LEFT_ARROW);
        syntheticKeys.leftArrowFrames--;
    }
    if (syntheticKeys.rightArrowFrames > 0)
    {
        keyboard.Press(HWKeyboard::RIGHT_ARROW);
        syntheticKeys.rightArrowFrames--;
    }
}


static void HandlePanMode(uint32_t nowMs)
{
    if (nowMs - touchBarSession.lastPanMs < TOUCHBAR_PAN_INTERVAL_MS)
        return;

    touchBarSession.lastPanMs = nowMs;

    const int16_t displacement = touchBarSession.currentPosition - touchBarSession.anchorPosition;
    const int16_t distance = abs16(displacement);
    if (distance <= TOUCHBAR_PAN_DEADZONE)
        return;

    int16_t speed = 1 + (distance - TOUCHBAR_PAN_DEADZONE) / (TOUCHBAR_POSITION_SCALE / 2);
    if (speed > 6)
        speed = 6;

    QueueMousePan((int8_t) (displacement > 0 ? speed : -speed));
}


static void QueueAppSwitchStep(int16_t direction)
{
    syntheticKeys.holdLeftAlt = true;
    syntheticKeys.tabFrames = SYNTHETIC_PULSE_FRAMES;

    if (direction < 0)
        syntheticKeys.leftShiftFrames = SYNTHETIC_PULSE_FRAMES;
}


static void HandleAppSwitchMode(uint32_t nowMs)
{
    const int16_t displacement = touchBarSession.currentPosition - touchBarSession.anchorPosition;
    const int16_t targetSteps = displacement / TOUCHBAR_STEP_DISTANCE;

    if (targetSteps == touchBarSession.emittedSteps)
        return;
    if (nowMs - touchBarSession.lastStepMs < TOUCHBAR_STEP_INTERVAL_MS)
        return;

    touchBarSession.lastStepMs = nowMs;

    if (targetSteps > touchBarSession.emittedSteps)
    {
        QueueAppSwitchStep(1);
        touchBarSession.emittedSteps++;
    }
    else
    {
        QueueAppSwitchStep(-1);
        touchBarSession.emittedSteps--;
    }
}


static void QueueDesktopSwitchStep(int16_t direction)
{
    syntheticKeys.leftCtrlFrames = SYNTHETIC_PULSE_FRAMES;
    syntheticKeys.leftGuiFrames = SYNTHETIC_PULSE_FRAMES;

    if (direction < 0)
        syntheticKeys.leftArrowFrames = SYNTHETIC_PULSE_FRAMES;
    else
        syntheticKeys.rightArrowFrames = SYNTHETIC_PULSE_FRAMES;
}


static void FinalizeDesktopSwitchGesture()
{
    if (!touchBarSession.isGestureActive || touchBarSession.isDesktopSeekMode)
        return;

    const int16_t displacement = touchBarSession.currentPosition - touchBarSession.anchorPosition;
    if (abs16(displacement) < TOUCHBAR_DESKTOP_SWIPE_DISTANCE)
        return;

    QueueDesktopSwitchStep(displacement > 0 ? 1 : -1);
}


static void HandleDesktopSwitchMode(uint32_t nowMs)
{
    if (!touchBarSession.isDesktopSeekMode)
    {
        if (nowMs - touchBarSession.touchStartMs < TOUCHBAR_DESKTOP_HOLD_MS)
            return;

        touchBarSession.isDesktopSeekMode = true;
        touchBarSession.anchorPosition = touchBarSession.currentPosition;
        touchBarSession.emittedSteps = 0;
        touchBarSession.lastStepMs = nowMs;
        return;
    }

    const int16_t displacement = touchBarSession.currentPosition - touchBarSession.anchorPosition;
    const int16_t targetSteps = displacement / TOUCHBAR_STEP_DISTANCE;

    if (targetSteps == touchBarSession.emittedSteps)
        return;
    if (nowMs - touchBarSession.lastStepMs < TOUCHBAR_STEP_INTERVAL_MS)
        return;

    touchBarSession.lastStepMs = nowMs;

    if (targetSteps > touchBarSession.emittedSteps)
    {
        QueueDesktopSwitchStep(1);
        touchBarSession.emittedSteps++;
    }
    else
    {
        QueueDesktopSwitchStep(-1);
        touchBarSession.emittedSteps--;
    }
}


static void ProcessTouchBar(uint32_t nowMs)
{
    const uint8_t touchState = keyboard.GetTouchBarState();
    const int16_t touchPosition = GetTouchBarPosition(touchState);

    if (touchPosition < 0)
    {
        if (touchBarSession.mode == TOUCHBAR_MODE_DESKTOP_SWITCH)
            FinalizeDesktopSwitchGesture();
        ClearTouchBarActions();
        return;
    }

    touchBarSession.currentPosition = touchPosition;

    if (!touchBarSession.isTouching)
    {
        touchBarSession.isTouching = true;
        touchBarSession.touchStartMs = nowMs;
        touchBarSession.anchorPosition = touchPosition;
        touchBarSession.currentPosition = touchPosition;
        touchBarSession.emittedSteps = 0;
        touchBarSession.isDesktopSeekMode = false;
        touchBarSession.lastPanMs = nowMs;
        touchBarSession.lastStepMs = nowMs;
        syntheticKeys.holdLeftAlt = false;
        return;
    }

    if (!touchBarSession.isGestureActive)
    {
        if (nowMs - touchBarSession.touchStartMs < TOUCHBAR_ACTIVATION_MS)
            return;

        touchBarSession.isGestureActive = true;
    }

    switch (touchBarSession.mode)
    {
        case TOUCHBAR_MODE_PAN:
            HandlePanMode(nowMs);
            break;
        case TOUCHBAR_MODE_APP_SWITCH:
            HandleAppSwitchMode(nowMs);
            break;
        case TOUCHBAR_MODE_DESKTOP_SWITCH:
            HandleDesktopSwitchMode(nowMs);
            break;
        default:
            break;
    }
}


static void SendPendingReports()
{
    if (hasPendingMouseReport)
    {
        if (USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS,
                                       pendingMouseReport,
                                       HWKeyboard::MOUSE_REPORT_SIZE) == USBD_OK)
            hasPendingMouseReport = false;
        return;
    }

    uint8_t* keyboardReport = keyboard.GetHidReportBuffer(1);
    if (!hasKeyboardReportSnapshot ||
        memcmp(lastKeyboardReport, keyboardReport, HWKeyboard::KEY_REPORT_SIZE) != 0)
    {
        if (USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS,
                                       keyboardReport,
                                       HWKeyboard::KEY_REPORT_SIZE) == USBD_OK)
        {
            memcpy(lastKeyboardReport, keyboardReport, HWKeyboard::KEY_REPORT_SIZE);
            hasKeyboardReportSnapshot = true;
        }
    }
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


/* LED position utilities ----------------------------------------------------*/
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
        /* Rainbow Sweep: full-keyboard rainbow with faster brightness wave */
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

        /* Reactive: keys flash on press and fade (~0.8s) */
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

        /* Aurora: multi-layered flowing ocean waves (Pacifica-inspired) */
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

        /* Contour: 3D value noise field, time as z-dimension */
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

        default:
            break;
    }
}


/* Status Indicator LEDs (82-84, near arrow keys) ---------------------------*/
static const uint8_t STATUS_LED_START = 82;
static const uint8_t STATUS_LED_COUNT = 3;

static void UpdateStatusLEDs()
{
    const HWKeyboard::Color_t baseColor = keyboard.isCapsLocked
                                          ? HWKeyboard::Color_t{180, 0, 255}
                                          : HWKeyboard::Color_t{255, 255, 255};
    HWKeyboard::Color_t currentColor = baseColor;

    if (statusBlink.active)
    {
        const uint32_t phase = (HAL_GetTick() - statusBlink.startMs) / STATUS_BLINK_PHASE_MS;
        if (phase >= (uint32_t) statusBlink.flashCount * 2)
            statusBlink.active = false;
        else if ((phase & 0x01U) == 0)
            currentColor = HWKeyboard::Color_t{0, 255, 0};
    }

    for (uint8_t i = 0; i < STATUS_LED_COUNT; i++)
        keyboard.SetRgbBufferByID(STATUS_LED_START + i, currentColor);
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
        if (!isSoftWareControlColor)
        {
            RenderLightEffect();
        }

        UpdateStatusLEDs();

        if (isReceiveSuccess)
        {
            keyboard.SyncLights();
            isReceiveSuccess = false;
        }
        else
        {
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

    keyboard.Remap(1);
    bool fnPressed = keyboard.FnPressed();
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
        if (keyboard.KeyPressed(HWKeyboard::RIGHT_CTRL))   curFnCombo |= 0x080;

        uint16_t justPressed = curFnCombo & ~prevFnCombo;

        if (justPressed & 0x001) keyboard.IncreaseBrightness();
        if (justPressed & 0x002) keyboard.DecreaseBrightness();
        if (justPressed & 0x004) keyboard.NextEffect();
        if (justPressed & 0x008) keyboard.SetEffect(HWKeyboard::EFFECT_RAINBOW_SWEEP);
        if (justPressed & 0x010) keyboard.SetEffect(HWKeyboard::EFFECT_REACTIVE);
        if (justPressed & 0x020) keyboard.SetEffect(HWKeyboard::EFFECT_AURORA);
        if (justPressed & 0x040) keyboard.SetEffect(HWKeyboard::EFFECT_CONTOUR);
        if (justPressed & 0x080) CycleTouchBarMode();

        prevFnCombo = curFnCombo;
        ClearTouchBarActions();

        uint8_t* report = keyboard.GetHidReportBuffer(1);
        memset(report + 1, 0, HWKeyboard::KEY_REPORT_SIZE - 1);
    }
    else
    {
        prevFnCombo = 0;
        ProcessTouchBar(HAL_GetTick());
        ApplySyntheticKeys();
    }

    SendPendingReports();
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