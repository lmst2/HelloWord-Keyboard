#include "common_inc.h"
#include "Platform/Sensor/Encoder/encoder.h"
#include "Platform/Driver/driver.h"
#include "Ctrl/Motor/motor.h"
#include "eink_290_bw.h"
#include "rgb_light.h"
#include "Ctrl/Motor/knob.h"
#include "U8g2lib.hpp"
#include "comm/protocol.h"
#include "comm/hub_uart_comm.h"
#include "comm/hub_usb_comm.h"
#include "comm/config_cache.h"
#include "config/hub_config.h"
#include "config/profile_store.h"
#include "app/app_manager.h"
#include "app/apps_knob/app_volume.h"
#include "app/apps_knob/app_scroll.h"
#include "app/apps_knob/app_switch_app.h"
#include "app/apps_knob/app_brightness.h"
#include "app/apps_knob/app_arrow.h"
#include "app/apps_knob/app_cpu_meter.h"
#include "app/apps_eink/app_eink_image.h"
#include "app/apps_eink/app_eink_stats.h"
#include "app/apps_eink/app_eink_info.h"
#include "app/apps_eink/app_eink_scroll.h"
#include "app/apps_eink/app_eink_calendar.h"
#include "app/apps_eink/app_eink_quote.h"
#include "app/apps_system/app_settings.h"
#include "app/apps_system/app_about.h"
#include "app/apps_system/app_profiles.h"
#include "input/input_manager.h"
#include "display/oled_display.h"
#include "display/eink_driver.h"
#include "display/display_manager.h"
#include "power/power_manager.h"


/* Global Instances ----------------------------------------------------------*/
BoardConfig_t boardConfig;
SSD1306 oled(&hi2c1);
Eink290BW eink;
Timer timerCtrlLoop(&htim7, 5000);
Encoder encoder(&hspi1);
Driver driver(12);
Motor motor = Motor(7);
RGB rgb(&hspi3);
KnobSimulator knob;

// Built-in knob apps
static AppVolume appVolume;
static AppScroll appScroll;
static AppSwitchApp appSwitchApp;
static AppBrightness appBrightness;
static AppArrowV appArrowV;
static AppArrowH appArrowH;
static AppCpuMeter appCpuMeter;

// Built-in E-Ink apps
static AppEinkImage appEinkImage;
static AppEinkStats appEinkStats;
static AppEinkInfoPanel appEinkInfo;
static AppEinkScrollText appEinkScroll;
static AppEinkCalendar appEinkCalendar;
static AppEinkQuote appEinkQuote;

// System apps
static AppSettings appSettings;
static AppAbout appAbout;
static AppProfiles appProfiles;


/* Thread: Control Loop (Realtime, 5ms) --------------------------------------*/
osThreadId_t ctrlLoopTaskHandle;
static int lastEncoderPos = 0;

void ThreadCtrlLoop(void* argument)
{
    motor.AttachDriver(&driver);
    motor.AttachEncoder(&encoder);
    knob.Init(&motor);
    knob.SetEnable(true);
    knob.SetMode(KnobSimulator::MODE_ENCODER);

    IWDG->KR  = 0x5555U;
    IWDG->PR  = 4;
    IWDG->RLR = 0xFFFU;
    IWDG->KR  = 0xCCCCU;

    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        knob.Tick();

        int newPos = knob.GetEncoderModePos();
        int32_t delta = newPos - lastEncoderPos;
        if (delta != 0) {
            lastEncoderPos = newPos;
            appManager.OnKnobDelta(delta);
            powerManager.OnActivity();
        }

        // CpuMeter motor output: override motor target when active
        IApp* primary = appManager.GetPrimaryApp();
        if (primary && primary->GetId() == appCpuMeter.GetId()) {
            appCpuMeter.OnMotorTick(HAL_GetTick());
        }

        IWDG->KR = 0xAAAAU;
    }
}


/* Thread: UART Communication ------------------------------------------------*/
osThreadId_t uartCommTaskHandle;
static void ThreadUartComm(void* argument)
{
    static const uint16_t DMA_BUF_SIZE = 128;
    static uint8_t dmaBuf[DMA_BUF_SIZE];
    uint32_t lastIdx = 0;

    HAL_UART_Receive_DMA(&huart2, dmaBuf, DMA_BUF_SIZE);
    lastIdx = DMA_BUF_SIZE - huart2.hdmarx->Instance->NDTR;

    osDelay(500);
    hubUart.SendStatusReq();
    hubUart.SendConfigGetAll();

    for (;;) {
        if (huart2.ErrorCode != HAL_UART_ERROR_NONE) {
            HAL_UART_AbortReceive(&huart2);
            HAL_UART_Receive_DMA(&huart2, dmaBuf, DMA_BUF_SIZE);
        }

        uint32_t newIdx = DMA_BUF_SIZE - huart2.hdmarx->Instance->NDTR;
        if (newIdx < lastIdx) {
            hubUart.ProcessBytes(dmaBuf + lastIdx, DMA_BUF_SIZE - lastIdx);
            lastIdx = 0;
        }
        if (newIdx > lastIdx) {
            hubUart.ProcessBytes(dmaBuf + lastIdx, newIdx - lastIdx);
            lastIdx = newIdx;
        }

        osDelay(1);
    }
}


/* Thread: Display + Input (~20fps) ------------------------------------------*/
osThreadId_t oledTaskHandle;
void ThreadOledUpdate(void* argument)
{
    oled.Init();
    eink.Init();
    einkDriver.Init(&eink);
    oledDisplay.Init(&oled, &appManager);
    displayManager.Init(&einkDriver, &oledDisplay, &appManager);
    inputManager.Init(&appManager);

    for (;;) {
        uint32_t nowMs = HAL_GetTick();

        // Button input
        bool keyA = HAL_GPIO_ReadPin(KEY_A_GPIO_Port, KEY_A_Pin) == GPIO_PIN_RESET;
        bool keyB = HAL_GPIO_ReadPin(KEY_B_GPIO_Port, KEY_B_Pin) == GPIO_PIN_RESET;
        inputManager.Update(keyA, keyB, nowMs);
        if (keyA || keyB) powerManager.OnActivity();

        // OLED render
        if (powerManager.GetOledBrightness() > 0) {
            displayManager.TickOled(nowMs);
        } else {
            oled.Clear();
            oled.SendBuffer();
        }

        // E-Ink render (rate-limited internally to ~0.5Hz)
        displayManager.TickEink(nowMs);

        // RGB indicator
        if (powerManager.GetState() != PowerManager::STANDBY) {
            for (uint8_t i = 0; i < RGB::LED_NUMBER; i++)
                rgb.SetRgbBuffer(i, RGB::Color_t{128, 20, 0});
        } else {
            for (uint8_t i = 0; i < RGB::LED_NUMBER; i++)
                rgb.SetRgbBuffer(i, RGB::Color_t{0, 0, 0});
        }
        rgb.SyncLights();

        // App tick
        appManager.Tick(nowMs);
        powerManager.Tick(nowMs);

        osDelay(50);
    }
}


/* Timer Callback (5ms) for motor control ------------------------------------*/
void OnTimerCallback()
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(TaskHandle_t(ctrlLoopTaskHandle), &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


/* App Registration ----------------------------------------------------------*/
static void RegisterAllApps()
{
    // Knob apps
    appManager.RegisterApp(&appVolume);
    appManager.RegisterApp(&appScroll);
    appManager.RegisterApp(&appSwitchApp);
    appManager.RegisterApp(&appBrightness);
    appManager.RegisterApp(&appArrowV);
    appManager.RegisterApp(&appArrowH);
    appManager.RegisterApp(&appCpuMeter);
    // E-Ink apps
    appManager.RegisterApp(&appEinkImage);
    appManager.RegisterApp(&appEinkStats);
    appManager.RegisterApp(&appEinkInfo);
    appManager.RegisterApp(&appEinkScroll);
    appManager.RegisterApp(&appEinkCalendar);
    appManager.RegisterApp(&appEinkQuote);
    // System apps
    appManager.RegisterApp(&appSettings);
    appManager.RegisterApp(&appAbout);
    appManager.RegisterApp(&appProfiles);
}


/* Main Entry ----------------------------------------------------------------*/
void Main(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_DBP;
    RTC->BKP1R = 0;

    hubConfig.Init();
    hubUart.Init();
    profileStore.Init();
    powerManager.Init(30000, 60000);

    RegisterAllApps();

    // Set initial active apps from saved config
    appManager.SetPrimaryApp(hubConfig.Data().primaryAppId);
    appManager.SetEinkApp(hubConfig.Data().einkAppId);
    if (appManager.GetPrimaryApp())
        appManager.GetPrimaryApp()->OnActivate();
    if (appManager.GetEinkApp())
        appManager.GetEinkApp()->OnEinkActivate();

    InitCommunication();

    const osThreadAttr_t ctrlAttr = {
        .name = "ControlLoopTask", .stack_size = 4096,
        .priority = (osPriority_t)osPriorityRealtime,
    };
    ctrlLoopTaskHandle = osThreadNew(ThreadCtrlLoop, nullptr, &ctrlAttr);

    const osThreadAttr_t uartAttr = {
        .name = "UartCommTask", .stack_size = 2048,
        .priority = (osPriority_t)osPriorityAboveNormal,
    };
    uartCommTaskHandle = osThreadNew(ThreadUartComm, nullptr, &uartAttr);

    const osThreadAttr_t oledAttr = {
        .name = "OledTask", .stack_size = 4096,
        .priority = (osPriority_t)osPriorityNormal,
    };
    oledTaskHandle = osThreadNew(ThreadOledUpdate, nullptr, &oledAttr);

    timerCtrlLoop.SetCallback(OnTimerCallback);
    timerCtrlLoop.Start();
}


extern "C" void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef* hspi)
{
    rgb.isRgbTxBusy = false;
}
