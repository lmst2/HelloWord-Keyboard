#include <cstring>
#include "common_inc.h"
#include "configurations.h"
#include "HelloWord/hw_keyboard.h"
#include "comm/protocol.h"
#include "comm/config_handler.h"
#include "comm/uart_comm.h"
#include "comm/usb_comm.h"
#include "effects/light_effects.h"
#include "features/touchbar.h"
#include "features/sleep.h"
#include "features/key_injector.h"


/* Global Instances ----------------------------------------------------------*/
KeyboardConfig_t config;
HWKeyboard keyboard(&hspi1);
ConfigHandler configHandler;
bool isSoftWareControlColor = false;


/* Status Indicator LEDs (82-84) ---------------------------------------------*/
static constexpr uint8_t STATUS_LED_START = 82;
static constexpr uint8_t STATUS_LED_COUNT = 3;
static constexpr uint32_t STATUS_BLINK_PHASE_MS = 120;

struct StatusBlinkState_t
{
    bool active = false;
    uint8_t flashCount = 0;
    uint32_t startMs = 0;
};
static StatusBlinkState_t statusBlink;
static volatile uint8_t pendingStatusBlinkCount = 0;


/* HID report tracking -------------------------------------------------------*/
static uint8_t lastKeyboardReport[HWKeyboard::KEY_REPORT_SIZE]{};
static bool hasKeyboardReportSnapshot = false;


/* Status LED rendering ------------------------------------------------------*/
static void UpdateStatusLEDs(uint32_t nowMs)
{
    uint8_t requestedBlink = pendingStatusBlinkCount;
    if (requestedBlink > 0) {
        statusBlink = {true, requestedBlink, nowMs};
        pendingStatusBlinkCount = 0;
    }

    if (sleepManager.IsSleeping()) {
        sleepManager.RenderStatusLeds(keyboard, nowMs, STATUS_LED_START, STATUS_LED_COUNT);
        return;
    }

    if (keyboard.brightnessLevel == 0) {
        for (uint8_t i = 0; i < STATUS_LED_COUNT; i++)
            keyboard.SetRgbBufferByID(STATUS_LED_START + i, {0, 0, 0});
        return;
    }

    HWKeyboard::Color_t baseColor = keyboard.isCapsLocked
        ? HWKeyboard::Color_t{180, 0, 255}
        : HWKeyboard::Color_t{255, 255, 255};
    HWKeyboard::Color_t currentColor = baseColor;

    if (statusBlink.active) {
        uint32_t phase = (nowMs - statusBlink.startMs) / STATUS_BLINK_PHASE_MS;
        if (phase >= (uint32_t)statusBlink.flashCount * 2)
            statusBlink.active = false;
        else if ((phase & 1U) == 0)
            currentColor = {0, 255, 0};
    }

    for (uint8_t i = 0; i < STATUS_LED_COUNT; i++)
        keyboard.SetRgbBufferByID(STATUS_LED_START + i, currentColor);
}


/* HID report sending --------------------------------------------------------*/
static void SendPendingReports()
{
    uint8_t* keyboardReport = keyboard.GetHidReportBuffer(1);
    bool currentShiftHeld = (keyboardReport[1] & (1U << 1)) != 0;
    bool lastShiftHeld = (lastKeyboardReport[1] & (1U << 1)) != 0;

    if (TouchBar_HasPendingMouseReport() && lastShiftHeld && !currentShiftHeld) {
        TouchBar_TrySendMouseReport();
        return;
    }

    if (!hasKeyboardReportSnapshot ||
        memcmp(lastKeyboardReport, keyboardReport, HWKeyboard::KEY_REPORT_SIZE) != 0) {
        if (USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, keyboardReport,
                                        HWKeyboard::KEY_REPORT_SIZE) == USBD_OK) {
            memcpy(lastKeyboardReport, keyboardReport, HWKeyboard::KEY_REPORT_SIZE);
            hasKeyboardReportSnapshot = true;
        }
        return;
    }

    if (TouchBar_HasPendingMouseReport()) {
        TouchBar_TrySendMouseReport();
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

    // Load config from EEPROM
    EEPROM eeprom;
    eeprom.Pull(0, config);
    if (config.configStatus != CONFIG_OK) {
        config = GetDefaultConfig();
        eeprom.Push(0, config);
    }

    // Initialize subsystems
    configHandler.Init(&config);
    configHandler.ApplyToRuntime();
    touchBar.Init(&config.touchBar);
    touchBar.SetMode((TouchBarMode_t)config.touchBar.mode);
    sleepManager.Init(&config.sleep);
    uartComm.Init();

    HAL_TIM_Base_Start_IT(&htim4);

    while (true) {
        uint32_t nowMs = HAL_GetTick();

        // Poll UART for decoded SLIP frames
        uartComm.Poll();

        // Render lighting
        if (!isSoftWareControlColor)
            RenderLightEffect(keyboard, config.effectColors);

        // Apply sleep dimming to main LEDs (skips status LEDs handled separately)
        sleepManager.ApplyLighting(keyboard, nowMs);
        UpdateStatusLEDs(nowMs);

        keyboard.SyncLights();
        IWDG->KR = 0xAAAA;
    }
}


/* 1kHz Timer Callback -------------------------------------------------------*/
extern "C" void OnTimerCallback()
{
    keyboard.ScanKeyStates();
    keyboard.ApplyDebounceFilter(100);
    keyboard.ApplyKeyDebounce(8);

    keyboard.Remap(config.activeLayer);
    bool fnPressed = keyboard.FnPressed();
    keyboard.UpdateKeyPressState();
    uint32_t nowMs = HAL_GetTick();
    sleepManager.Update(nowMs, keyboard.HasAnyPhysicalInput());

    static uint16_t prevFnCombo = 0;
    static bool prevTouchBarModeSwitchPressed = false;

    if (fnPressed) {
        uint16_t curFnCombo = 0;
        bool tbModeSwitchPressed = keyboard.KeyPressed(HWKeyboard::RIGHT_CTRL);

        if (keyboard.KeyPressed(HWKeyboard::UP_ARROW))   curFnCombo |= 0x001;
        if (keyboard.KeyPressed(HWKeyboard::DOWN_ARROW)) curFnCombo |= 0x002;
        if (keyboard.KeyPressed(HWKeyboard::SPACE))       curFnCombo |= 0x004;
        if (keyboard.KeyPressed(HWKeyboard::NUM_1))       curFnCombo |= 0x008;
        if (keyboard.KeyPressed(HWKeyboard::NUM_2))       curFnCombo |= 0x010;
        if (keyboard.KeyPressed(HWKeyboard::NUM_3))       curFnCombo |= 0x020;
        if (keyboard.KeyPressed(HWKeyboard::NUM_4))       curFnCombo |= 0x040;
        if (keyboard.KeyPressed(HWKeyboard::NUM_5))       curFnCombo |= 0x080;

        uint16_t justPressed = curFnCombo & ~prevFnCombo;

        if (justPressed & 0x001) keyboard.IncreaseBrightness();
        if (justPressed & 0x002) keyboard.DecreaseBrightness();
        if (justPressed & 0x004) keyboard.NextEffect();
        if (justPressed & 0x008) keyboard.SetEffect(HWKeyboard::EFFECT_RAINBOW_SWEEP);
        if (justPressed & 0x010) keyboard.SetEffect(HWKeyboard::EFFECT_REACTIVE);
        if (justPressed & 0x020) keyboard.SetEffect(HWKeyboard::EFFECT_AURORA);
        if (justPressed & 0x040) keyboard.SetEffect(HWKeyboard::EFFECT_RIPPLE);
        if (justPressed & 0x080) keyboard.SetEffect(HWKeyboard::EFFECT_STATIC);
        if (tbModeSwitchPressed && !prevTouchBarModeSwitchPressed) touchBar.CycleMode();

        prevFnCombo = curFnCombo;
        prevTouchBarModeSwitchPressed = tbModeSwitchPressed;
        touchBar.ClearActions();

        uint8_t* report = keyboard.GetHidReportBuffer(1);
        memset(report + 1, 0, HWKeyboard::KEY_REPORT_SIZE - 1);
    } else {
        prevFnCombo = 0;
        prevTouchBarModeSwitchPressed = false;
        touchBar.Process(nowMs, keyboard);
    }

    // Inject keys from Hub (knob functions)
    keyInjector.ProcessFrame(keyboard);

    // Check for status blink from touchbar mode switch
    uint8_t blink = touchBar.ConsumePendingBlink();
    if (blink > 0) pendingStatusBlinkCount = blink;

    SendPendingReports();

    // Send Fn state to Hub
    static bool prevFnState = false;
    if (fnPressed != prevFnState) {
        prevFnState = fnPressed;
        uint8_t payload = fnPressed ? 1 : 0;
        uartComm.Send(Msg::KB_HUB_FN_STATE, &payload, 1);
    }
}


/* USART1 RXNE ISR -----------------------------------------------------------*/
extern "C" void OnUartCmd(uint8_t* _data, uint16_t _len)
{
    // Legacy stub — actual UART processing uses RXNE interrupt + SLIP
    (void)_data; (void)_len;
}


/* SPI DMA Complete -----------------------------------------------------------*/
extern "C" void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef* hspi)
{
    keyboard.isRgbTxBusy = false;
}


/* USB HID Receive ------------------------------------------------------------*/
extern "C" void HID_RxCpltCallback(uint8_t* _data)
{
    usbComm.OnHidReport(_data);
}
