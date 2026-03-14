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
    bool isNoTouchPending = false;
    uint32_t touchStartMs = 0;
    uint32_t lastTouchMs = 0;
    uint32_t lastPanMs = 0;
    uint32_t lastStepMs = 0;
    int16_t anchorPosition = 0;
    int16_t currentPosition = 0;
    int16_t emittedSteps = 0;
};

struct SyntheticKeyState_t
{
    bool holdLeftAlt = false;
    bool holdLeftShift = false;
    bool hasShiftScrollPrimed = false;
    uint8_t leftShiftFrames = 0;
    uint8_t leftCtrlFrames = 0;
    uint8_t leftGuiFrames = 0;
    uint8_t tabDelayFrames = 0;
    uint8_t tabFrames = 0;
    uint8_t leftArrowDelayFrames = 0;
    uint8_t rightArrowDelayFrames = 0;
    uint8_t leftArrowFrames = 0;
    uint8_t rightArrowFrames = 0;
};

struct StatusBlinkState_t
{
    bool active = false;
    uint8_t flashCount = 0;
    uint32_t startMs = 0;
};

struct SleepState_t
{
    volatile bool isSleeping = false;
    volatile bool isFadeOutActive = false;
    volatile uint32_t lastActivityMs = 0;
    volatile uint32_t fadeStartMs = 0;
};

static TouchBarSession_t touchBarSession;
static SyntheticKeyState_t syntheticKeys;
static StatusBlinkState_t statusBlink;
static SleepState_t sleepState;
static uint8_t lastKeyboardReport[HWKeyboard::KEY_REPORT_SIZE]{};
static uint8_t pendingMouseReport[HWKeyboard::MOUSE_REPORT_SIZE]{};
static bool hasKeyboardReportSnapshot = false;
static bool hasPendingMouseReport = false;

static const uint8_t STATUS_LED_START = 82;
static const uint8_t STATUS_LED_COUNT = 3;
static const uint32_t TOUCHBAR_ACTIVATION_MS = 20;
static const uint32_t TOUCHBAR_DESKTOP_HOLD_MS = 1000;
static const uint32_t TOUCHBAR_RELEASE_GRACE_MS = 35;
static const uint32_t TOUCHBAR_PAN_INTERVAL_MS = 12;
static const uint32_t TOUCHBAR_STEP_INTERVAL_MS = 55;
static const uint32_t STATUS_BLINK_PHASE_MS = 120;
static const uint32_t SLEEP_IDLE_TIMEOUT_MS = 300000;
static const uint32_t SLEEP_FADE_OUT_MS = 800;
static const uint32_t SLEEP_BREATHE_PERIOD_MS = 1200;
static const int16_t TOUCHBAR_POSITION_SCALE = 256;
static const int16_t TOUCHBAR_DESKTOP_SWIPE_DISTANCE = 96;
static const int16_t TOUCHBAR_EDGE_REPEAT_THRESHOLD = 64;
static const int16_t TOUCHBAR_PAN_DEADZONE = 64;
static const int16_t TOUCHBAR_STEP_DISTANCE = 160;
static const int16_t TOUCHBAR_DESKTOP_STEP_DISTANCE = 192;
static const uint8_t SYNTHETIC_PULSE_FRAMES = 2;
static const float SLEEP_STATUS_MIN_BRIGHTNESS = 0.04f;
static const float SLEEP_STATUS_MAX_BRIGHTNESS = 0.14f;


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


static void UpdateSleepState(uint32_t nowMs, bool hasPhysicalInput)
{
    if (hasPhysicalInput)
    {
        sleepState.lastActivityMs = nowMs;
        sleepState.isFadeOutActive = false;
        sleepState.isSleeping = false;
        return;
    }

    if (sleepState.isSleeping)
        return;

    if (sleepState.isFadeOutActive)
    {
        if (nowMs - sleepState.fadeStartMs >= SLEEP_FADE_OUT_MS)
        {
            sleepState.isFadeOutActive = false;
            sleepState.isSleeping = true;
        }
        return;
    }

    if (nowMs - sleepState.lastActivityMs >= SLEEP_IDLE_TIMEOUT_MS)
    {
        sleepState.isFadeOutActive = true;
        sleepState.fadeStartMs = nowMs;
    }
}


static float GetSleepFadeScale(uint32_t nowMs)
{
    if (sleepState.isSleeping)
        return 0.0f;

    if (!sleepState.isFadeOutActive)
        return 1.0f;

    const uint32_t elapsed = nowMs - sleepState.fadeStartMs;
    if (elapsed >= SLEEP_FADE_OUT_MS)
        return 0.0f;

    return 1.0f - (float) elapsed / (float) SLEEP_FADE_OUT_MS;
}


static float GetSleepPulseBrightness(uint32_t nowMs)
{
    const uint32_t phaseMs = nowMs % SLEEP_BREATHE_PERIOD_MS;
    const uint8_t wave = sin8((uint8_t) (((phaseMs * 256UL) / SLEEP_BREATHE_PERIOD_MS) + 192UL));
    return SLEEP_STATUS_MIN_BRIGHTNESS +
           ((float) wave / 255.0f) * (SLEEP_STATUS_MAX_BRIGHTNESS - SLEEP_STATUS_MIN_BRIGHTNESS);
}


static void ClearTouchBarActions()
{
    touchBarSession.isTouching = false;
    touchBarSession.isGestureActive = false;
    touchBarSession.isDesktopSeekMode = false;
    touchBarSession.isNoTouchPending = false;
    touchBarSession.touchStartMs = 0;
    touchBarSession.lastTouchMs = 0;
    touchBarSession.lastPanMs = 0;
    touchBarSession.lastStepMs = 0;
    touchBarSession.anchorPosition = 0;
    touchBarSession.currentPosition = 0;
    touchBarSession.emittedSteps = 0;
    syntheticKeys.holdLeftAlt = false;
    syntheticKeys.holdLeftShift = false;
    syntheticKeys.hasShiftScrollPrimed = false;
}


static void CycleTouchBarMode()
{
    touchBarSession.mode = (TouchBarMode_t) ((touchBarSession.mode + 1) % TOUCHBAR_MODE_COUNT);
    TriggerStatusBlink((uint8_t) touchBarSession.mode + 1);
}


static void QueueMouseWheel(int8_t wheel)
{
    if (wheel == 0)
        return;

    keyboard.SetMouseWheel(wheel);
    memcpy(pendingMouseReport, keyboard.GetHidReportBuffer(3), HWKeyboard::MOUSE_REPORT_SIZE);
    hasPendingMouseReport = true;
    keyboard.ClearMouseReport();
}


static void ApplySyntheticKeys()
{
    if (syntheticKeys.holdLeftAlt)
        keyboard.Press(HWKeyboard::LEFT_ALT);
    if (syntheticKeys.holdLeftShift)
        keyboard.Press(HWKeyboard::LEFT_SHIFT);

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
    if (syntheticKeys.tabDelayFrames > 0)
    {
        syntheticKeys.tabDelayFrames--;
    }
    else if (syntheticKeys.tabFrames > 0)
    {
        keyboard.Press(HWKeyboard::TAB);
        syntheticKeys.tabFrames--;
    }
    if (syntheticKeys.leftArrowDelayFrames > 0)
    {
        syntheticKeys.leftArrowDelayFrames--;
    }
    else if (syntheticKeys.leftArrowFrames > 0)
    {
        keyboard.Press(HWKeyboard::LEFT_ARROW);
        syntheticKeys.leftArrowFrames--;
    }
    if (syntheticKeys.rightArrowDelayFrames > 0)
    {
        syntheticKeys.rightArrowDelayFrames--;
    }
    else if (syntheticKeys.rightArrowFrames > 0)
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

    syntheticKeys.holdLeftShift = true;
    if (!syntheticKeys.hasShiftScrollPrimed)
    {
        syntheticKeys.hasShiftScrollPrimed = true;
        return;
    }

    QueueMouseWheel((int8_t) (displacement > 0 ? -speed : speed));
}


static void QueueAppSwitchStep(int16_t direction)
{
    syntheticKeys.holdLeftAlt = true;
    syntheticKeys.tabDelayFrames = 1;
    syntheticKeys.tabFrames = SYNTHETIC_PULSE_FRAMES;

    if (direction < 0)
        syntheticKeys.leftShiftFrames = SYNTHETIC_PULSE_FRAMES + 1;
    else
        syntheticKeys.leftShiftFrames = 0;
}


static int16_t GetTouchBarEdgeDirection(int16_t position)
{
    const int16_t maxPosition = (HWKeyboard::TOUCHPAD_NUMBER - 1) * TOUCHBAR_POSITION_SCALE;
    if (position <= TOUCHBAR_EDGE_REPEAT_THRESHOLD)
        return -1;
    if (position >= maxPosition - TOUCHBAR_EDGE_REPEAT_THRESHOLD)
        return 1;
    return 0;
}


static bool TryRepeatStepAtEdge(uint32_t nowMs, void (*queueStep)(int16_t))
{
    const int16_t edgeDirection = GetTouchBarEdgeDirection(touchBarSession.currentPosition);
    if (edgeDirection == 0)
        return false;
    if (nowMs - touchBarSession.lastStepMs < TOUCHBAR_STEP_INTERVAL_MS)
        return true;

    touchBarSession.lastStepMs = nowMs;
    queueStep(edgeDirection);
    touchBarSession.emittedSteps += edgeDirection;
    // Move the anchor virtually outward so edge holds can continue stepping.
    touchBarSession.anchorPosition -= edgeDirection * TOUCHBAR_STEP_DISTANCE;
    return true;
}


static void HandleAppSwitchMode(uint32_t nowMs)
{
    const int16_t displacement = touchBarSession.currentPosition - touchBarSession.anchorPosition;
    const int16_t targetSteps = displacement / TOUCHBAR_STEP_DISTANCE;

    if (targetSteps == touchBarSession.emittedSteps)
    {
        TryRepeatStepAtEdge(nowMs, QueueAppSwitchStep);
        return;
    }
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
    syntheticKeys.leftCtrlFrames = SYNTHETIC_PULSE_FRAMES + 1;
    syntheticKeys.leftGuiFrames = SYNTHETIC_PULSE_FRAMES + 1;
    syntheticKeys.leftArrowDelayFrames = 0;
    syntheticKeys.rightArrowDelayFrames = 0;
    syntheticKeys.leftArrowFrames = 0;
    syntheticKeys.rightArrowFrames = 0;

    if (direction < 0)
    {
        syntheticKeys.leftArrowDelayFrames = 1;
        syntheticKeys.leftArrowFrames = SYNTHETIC_PULSE_FRAMES;
    }
    else
    {
        syntheticKeys.rightArrowDelayFrames = 1;
        syntheticKeys.rightArrowFrames = SYNTHETIC_PULSE_FRAMES;
    }
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
    const int16_t targetSteps = displacement / TOUCHBAR_DESKTOP_STEP_DISTANCE;

    if (targetSteps == touchBarSession.emittedSteps)
    {
        TryRepeatStepAtEdge(nowMs, QueueDesktopSwitchStep);
        return;
    }
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
        if (!touchBarSession.isTouching)
            return;

        if (!touchBarSession.isNoTouchPending)
        {
            touchBarSession.isNoTouchPending = true;
            touchBarSession.lastTouchMs = nowMs;
            return;
        }

        if (nowMs - touchBarSession.lastTouchMs < TOUCHBAR_RELEASE_GRACE_MS)
            return;

        if (touchBarSession.mode == TOUCHBAR_MODE_DESKTOP_SWITCH)
            FinalizeDesktopSwitchGesture();
        ClearTouchBarActions();
        return;
    }

    touchBarSession.isNoTouchPending = false;
    touchBarSession.lastTouchMs = nowMs;
    touchBarSession.currentPosition = touchPosition;

    if (!touchBarSession.isTouching)
    {
        touchBarSession.isTouching = true;
        touchBarSession.touchStartMs = nowMs;
        touchBarSession.lastTouchMs = nowMs;
        touchBarSession.anchorPosition = touchPosition;
        touchBarSession.currentPosition = touchPosition;
        touchBarSession.emittedSteps = 0;
        touchBarSession.isDesktopSeekMode = false;
        touchBarSession.lastPanMs = nowMs;
        touchBarSession.lastStepMs = nowMs;
        syntheticKeys.holdLeftAlt = false;
        syntheticKeys.holdLeftShift = false;
        syntheticKeys.hasShiftScrollPrimed = false;
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
    uint8_t* keyboardReport = keyboard.GetHidReportBuffer(1);
    const bool currentShiftHeld = (keyboardReport[1] & (1U << 1)) != 0;
    const bool lastShiftHeld = (lastKeyboardReport[1] & (1U << 1)) != 0;

    if (hasPendingMouseReport && lastShiftHeld && !currentShiftHeld)
    {
        if (USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS,
                                       pendingMouseReport,
                                       HWKeyboard::MOUSE_REPORT_SIZE) == USBD_OK)
            hasPendingMouseReport = false;
        return;
    }

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
        return;
    }

    if (hasPendingMouseReport)
    {
        if (USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS,
                                       pendingMouseReport,
                                       HWKeyboard::MOUSE_REPORT_SIZE) == USBD_OK)
            hasPendingMouseReport = false;
        return;
    }
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
                    keyboard.SetRgbBufferByID(i, {0, 0, 0});
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

        /* Ripple: colorful rings expand from pressed keys */
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
                    keyboard.SetRgbBufferByID(i, {0, 0, 0});
            }
            break;
        }

        /* Static: warm white */
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
static void ApplySleepLighting(uint32_t nowMs)
{
    const float sleepScale = GetSleepFadeScale(nowMs);
    if (sleepScale >= 1.0f)
        return;

    for (uint8_t i = 0; i < HWKeyboard::LED_NUMBER; i++)
    {
        if (i >= STATUS_LED_START && i < STATUS_LED_START + STATUS_LED_COUNT)
            continue;

        if (sleepScale <= 0.0f)
            keyboard.TurnOffRgbOutputByID(i);
        else
            keyboard.ApplyStoredRgbByID(i, sleepScale);
    }
}


static void UpdateStatusLEDs(uint32_t nowMs)
{
    if (sleepState.isSleeping)
    {
        const HWKeyboard::Color_t sleepColor{255, 255, 255};
        const float sleepBrightness = GetSleepPulseBrightness(nowMs);
        for (uint8_t i = 0; i < STATUS_LED_COUNT; i++)
            keyboard.SetRgbBufferByID(STATUS_LED_START + i, sleepColor, sleepBrightness);
        return;
    }

    if (keyboard.brightnessLevel == 0)
    {
        for (uint8_t i = 0; i < STATUS_LED_COUNT; i++)
            keyboard.TurnOffRgbOutputByID(STATUS_LED_START + i);
        return;
    }

    const HWKeyboard::Color_t baseColor = keyboard.isCapsLocked
                                          ? HWKeyboard::Color_t{180, 0, 255}
                                          : HWKeyboard::Color_t{255, 255, 255};
    HWKeyboard::Color_t currentColor = baseColor;

    if (statusBlink.active)
    {
        const uint32_t phase = (nowMs - statusBlink.startMs) / STATUS_BLINK_PHASE_MS;
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
        const uint32_t nowMs = HAL_GetTick();
        if (!isSoftWareControlColor)
        {
            RenderLightEffect();
        }

        ApplySleepLighting(nowMs);
        UpdateStatusLEDs(nowMs);

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
    const uint32_t nowMs = HAL_GetTick();
    UpdateSleepState(nowMs, keyboard.HasAnyPhysicalInput());

    static uint16_t prevFnCombo = 0;
    static bool prevTouchBarModeSwitchPressed = false;

    if (fnPressed)
    {
        uint16_t curFnCombo = 0;
        const bool touchBarModeSwitchPressed = keyboard.KeyPressed(HWKeyboard::RIGHT_CTRL);

        if (keyboard.KeyPressed(HWKeyboard::UP_ARROW))    curFnCombo |= 0x001;
        if (keyboard.KeyPressed(HWKeyboard::DOWN_ARROW))  curFnCombo |= 0x002;
        if (keyboard.KeyPressed(HWKeyboard::SPACE))        curFnCombo |= 0x004;
        if (keyboard.KeyPressed(HWKeyboard::NUM_1))        curFnCombo |= 0x008;
        if (keyboard.KeyPressed(HWKeyboard::NUM_2))        curFnCombo |= 0x010;
        if (keyboard.KeyPressed(HWKeyboard::NUM_3))        curFnCombo |= 0x020;
        if (keyboard.KeyPressed(HWKeyboard::NUM_4))        curFnCombo |= 0x040;
        if (keyboard.KeyPressed(HWKeyboard::NUM_5))        curFnCombo |= 0x080;

        uint16_t justPressed = curFnCombo & ~prevFnCombo;

        if (justPressed & 0x001) keyboard.IncreaseBrightness();
        if (justPressed & 0x002) keyboard.DecreaseBrightness();
        if (justPressed & 0x004) keyboard.NextEffect();
        if (justPressed & 0x008) keyboard.SetEffect(HWKeyboard::EFFECT_RAINBOW_SWEEP);
        if (justPressed & 0x010) keyboard.SetEffect(HWKeyboard::EFFECT_REACTIVE);
        if (justPressed & 0x020) keyboard.SetEffect(HWKeyboard::EFFECT_AURORA);
        if (justPressed & 0x040) keyboard.SetEffect(HWKeyboard::EFFECT_RIPPLE);
        if (justPressed & 0x080) keyboard.SetEffect(HWKeyboard::EFFECT_STATIC);
        if (touchBarModeSwitchPressed && !prevTouchBarModeSwitchPressed) CycleTouchBarMode();

        prevFnCombo = curFnCombo;
        prevTouchBarModeSwitchPressed = touchBarModeSwitchPressed;
        ClearTouchBarActions();

        uint8_t* report = keyboard.GetHidReportBuffer(1);
        memset(report + 1, 0, HWKeyboard::KEY_REPORT_SIZE - 1);
    }
    else
    {
        prevFnCombo = 0;
        prevTouchBarModeSwitchPressed = false;
        ProcessTouchBar(nowMs);
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