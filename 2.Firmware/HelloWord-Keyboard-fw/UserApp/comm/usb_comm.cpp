#include "usb_comm.h"
#include "protocol.h"
#include "kb_device_log.h"
#include "config_handler.h"
#include <cstdio>
#include "common_inc.h"
#include "configurations.h"
#include "HelloWord/hw_keyboard.h"

extern HWKeyboard keyboard;
extern ConfigHandler configHandler;
extern bool isSoftWareControlColor;

UsbComm usbComm;

static void RebootToDfu()
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
    PWR->CR |= PWR_CR_DBP;
    BKP->DR1 = 0xB011U;
    NVIC_SystemReset();
}

void UsbComm::OnHidReport(uint8_t* data)
{
    // Report ID 1: keyboard LED status from host
    if (data[0] == 1) {
        keyboard.isCapsLocked = data[1] & 0x02;
        return;
    }

    // Report ID 2: raw HID command. data[1] = command byte.
    HandleRawCommand(data);
}

bool UsbComm::SendRawReport(const uint8_t* data, uint8_t len)
{
    if (len > HWKeyboard::RAW_REPORT_SIZE) return false;

    uint8_t report[HWKeyboard::RAW_REPORT_SIZE]{};
    report[0] = 2;
    for (uint8_t i = 0; i < len && i < HWKeyboard::RAW_REPORT_SIZE - 1; i++)
        report[1 + i] = data[i];

    return USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, report, HWKeyboard::RAW_REPORT_SIZE) == USBD_OK;
}

void UsbComm::HandleRawCommand(uint8_t* data)
{
    uint8_t cmd = data[1];

    if (KbDeviceLogShouldEmit(2)) {
        char line[48];
        snprintf(line, sizeof(line), "HID cmd=0x%02X", cmd);
        KbDeviceLogLine(2, line);
    }

    switch (cmd) {
        case Msg::PC_KB_DFU:
            KbDeviceLogLine(1, "HID DFU request");
            RebootToDfu();
            break;

        case Msg::LEGACY_RGB_STOP:
            isSoftWareControlColor = false;
            KbDeviceLogLine(2, "HID RGB stop");
            break;

        case Msg::LEGACY_RGB_DIRECT: {
            isSoftWareControlColor = true;
            uint8_t pageIndex = data[2];
            for (uint8_t i = 0; i < 10; i++) {
                if (i + pageIndex * 10 >= HWKeyboard::LED_NUMBER) break;
                keyboard.SetRgbBufferByID(i + pageIndex * 10,
                    HWKeyboard::Color_t{data[3 + i * 3], data[4 + i * 3], data[5 + i * 3]});
            }
            break;
        }

        case Msg::PC_KB_RGB_MODE: {
            if (data[2] < HWKeyboard::EFFECT_COUNT) {
                keyboard.SetEffect((HWKeyboard::LightEffect_t)data[2]);
                isSoftWareControlColor = false;
            }
            break;
        }

        case Msg::PC_KB_CONFIG_GET:
            KbDeviceLogLine(2, "HID config get");
            HandleConfigGet(data);
            break;

        case Msg::PC_KB_CONFIG_SET:
            KbDeviceLogLine(2, "HID config set");
            HandleConfigSet(data);
            break;

        case Msg::PC_KB_CONFIG_GET_ALL:
            KbDeviceLogLine(2, "HID config get all");
            HandleConfigGetAll();
            break;

        case Msg::PC_KB_STATUS_REQ:
            KbDeviceLogLine(2, "HID status req");
            HandleStatusReq();
            break;

        default:
            KbDeviceLogLine(2, "HID unknown cmd");
            break;
    }
}

void UsbComm::HandleConfigGet(uint8_t* data)
{
    uint16_t paramId = ((uint16_t)data[2] << 8) | data[3];
    uint8_t val[4];
    uint8_t vlen = configHandler.GetParam(paramId, val);
    if (vlen > 0) {
        uint8_t resp[6];
        resp[0] = Msg::KB_PC_CONFIG_VALUE;
        resp[1] = data[2]; // paramId hi
        resp[2] = data[3]; // paramId lo
        for (uint8_t i = 0; i < vlen; i++) resp[3 + i] = val[i];
        SendRawReport(resp, 3 + vlen);
    }
}

void UsbComm::HandleConfigSet(uint8_t* data)
{
    uint16_t paramId = ((uint16_t)data[2] << 8) | data[3];
    bool ok = configHandler.SetParam(paramId, data + 4, 1);
    uint8_t resp[3] = {Msg::KB_PC_ACK, data[1],
                       ok ? Msg::RESULT_OK : Msg::RESULT_ERR_PARAM};
    SendRawReport(resp, 3);
    if (ok) configHandler.Save();
}

void UsbComm::HandleConfigGetAll()
{
    uint8_t buf[28];
    buf[0] = Msg::KB_PC_CONFIG_VALUE;
    uint16_t total = configHandler.GetAllParams(buf + 1, sizeof(buf) - 1);
    if (total > 0)
        SendRawReport(buf, 1 + (uint8_t)total);
}

void UsbComm::HandleStatusReq()
{
    extern KeyboardConfig_t config;
    uint8_t resp[8] = {
        Msg::KB_PC_STATUS,
        1, // firmware version
        config.effectMode,
        config.brightness,
        config.activeLayer,
        config.osMode,
        config.touchBar.mode,
        0
    };
    SendRawReport(resp, sizeof(resp));
}
