#include "hub_usb_comm.h"
#include "protocol.h"
#include "device_log.h"
#include <cstdio>
#include "hub_uart_comm.h"
#include "config/hub_config.h"
#include "config/profile_store.h"
#include "app/app_manager.h"
#include "common_inc.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"

HubUsbComm hubUsb;

// CDC_Transmit_FS rejects Len > 64; large replies (e.g. profile list) must be split.
// Tx buffer is reused until the IN transfer completes — brief delay between chunks.
bool HubUsbComm::SendCdc(const uint8_t* data, uint16_t len)
{
    uint16_t offset = 0;
    while (offset < len) {
        uint16_t chunk = len - offset;
        if (chunk > CDC_DATA_MAX_PACKET_SIZE) {
            chunk = CDC_DATA_MAX_PACKET_SIZE;
        }

        uint32_t busyWaits = 0;
        uint8_t st = USBD_FAIL;
        for (;;) {
            st = CDC_Transmit_FS((uint8_t*)(data + offset), chunk, CDC_IN_EP);
            if (st != USBD_BUSY) {
                break;
            }
            HAL_Delay(1);
            if (++busyWaits > 500) {
                return false;
            }
        }
        if (st != USBD_OK) {
            return false;
        }
        offset += chunk;
        if (offset < len) {
            HAL_Delay(3);
        }
    }
    return true;
}

bool HubUsbComm::SendResponse(const uint8_t* resp, uint16_t respLen)
{
    uint8_t pkt[260];
    if (respLen + 2 > sizeof(pkt)) return false;
    pkt[0] = (uint8_t)(respLen & 0xFF);
    pkt[1] = (uint8_t)(respLen >> 8);
    for (uint16_t i = 0; i < respLen; i++) pkt[2 + i] = resp[i];
    return SendCdc(pkt, 2 + respLen);
}

void HubUsbComm::OnCdcData(const uint8_t* data, uint16_t len)
{
    if (len < 3) return;
    uint16_t msgLen = data[0] | ((uint16_t)data[1] << 8);
    if (msgLen > len - 2) return;
    HandleCommand(data + 2, msgLen);
}

void HubUsbComm::HandleCommand(const uint8_t* data, uint16_t len)
{
    if (len < 1) return;

    uint8_t cmd = data[0];
    const uint8_t* payload = data + 1;
    const uint16_t payloadLen = (len >= 1) ? (uint16_t)(len - 1) : 0;

    if (DeviceLogShouldEmit(3)) {
        char line[72];
        snprintf(line, sizeof(line), "hub CDC in cmd=0x%02X pl=%u", cmd, (unsigned)payloadLen);
        DeviceLogEmitHub(3, line);
    }

    switch (cmd) {
        case Msg::PC_HUB_CONFIG_GET:     HandleConfigGet(payload, payloadLen); break;
        case Msg::PC_HUB_CONFIG_SET:     HandleConfigSet(payload, payloadLen); break;
        case Msg::PC_HUB_CONFIG_GET_ALL: HandleConfigGetAll(payload, payloadLen); break;
        case Msg::PC_HUB_STATUS_REQ:     HandleStatusReq(payload, payloadLen); break;
        case Msg::PC_HUB_DATA_FEED:      HandleDataFeed(payload, payloadLen); break;
        case Msg::PC_HUB_EINK_IMAGE:     HandleEinkImage(payload, len - 1); break;
        case Msg::PC_HUB_EINK_TEXT:      HandleEinkText(payload, payloadLen); break;
        case Msg::PC_HUB_FW_INFO_REQ:    HandleFwInfoReq(); break;
        case Msg::PC_HUB_PROFILE_LIST:   HandleProfileList(); break;
        case Msg::PC_HUB_PROFILE_SAVE:   HandleProfileSave(payload, payloadLen); break;
        case Msg::PC_HUB_PROFILE_LOAD:   HandleProfileLoad(payload, payloadLen); break;
        case Msg::PC_HUB_PROFILE_DELETE:  HandleProfileDelete(payload, payloadLen); break;
        case Msg::PC_HUB_APP_SWITCH:     HandleAppSwitch(payload, payloadLen); break;
        case Msg::PC_HUB_EINK_SWITCH:    HandleEinkSwitch(payload, payloadLen); break;
        case Msg::PC_HUB_DFU_KB:
            hubUart.Send(Msg::HUB_KB_DFU);
            break;
        case Msg::PC_HUB_DFU_HUB: {
            RCC->APB1ENR |= RCC_APB1ENR_PWREN;
            PWR->CR |= PWR_CR_DBP;
            RTC->BKP1R = 0xDEADBEEF;
            NVIC_SystemReset();
            break;
        }
        case Msg::PC_HUB_RGB_FORWARD:
            if (payloadLen >= 2) {
                uint16_t rgbLen = payloadLen - 1;
                uint8_t uartLen = rgbLen > 255 ? (uint8_t)255 : (uint8_t)rgbLen;
                hubUart.Send(payload[0], payload + 1, uartLen);
            }
            break;
        case Msg::PC_HUB_LOG_CONFIG:
            if (payloadLen >= 2)
                DeviceLogApplyFromPc(payload[0], payload[1]);
            break;
        default: break;
    }
}

void HubUsbComm::HandleConfigGet(const uint8_t* payload, uint8_t len)
{
    if (len < 3) return;
    uint8_t target = payload[0];
    uint16_t paramId = ((uint16_t)payload[1] << 8) | payload[2];

    if (target == Msg::TARGET_KEYBOARD) {
        hubUart.SendConfigGet(paramId);
    } else {
        uint8_t val[4];
        uint8_t vlen = hubConfig.GetParam(paramId, val);
        if (vlen > 0) {
            uint8_t resp[8] = {Msg::HUB_PC_CONFIG_VALUE, Msg::TARGET_HUB,
                               (uint8_t)(paramId >> 8), (uint8_t)(paramId & 0xFF)};
            for (uint8_t i = 0; i < vlen; i++) resp[4 + i] = val[i];
            SendResponse(resp, 4 + vlen);
        }
    }
}

void HubUsbComm::HandleConfigSet(const uint8_t* payload, uint8_t len)
{
    if (len < 4) return;
    uint8_t target = payload[0];
    uint16_t paramId = ((uint16_t)payload[1] << 8) | payload[2];

    if (target == Msg::TARGET_KEYBOARD) {
        hubUart.SendConfigSet(paramId, payload + 3, len - 3);
    } else {
        bool ok = hubConfig.SetParam(paramId, payload + 3, len - 3);
        uint8_t resp[3] = {Msg::HUB_PC_ACK, Msg::PC_HUB_CONFIG_SET,
                           ok ? Msg::RESULT_OK : Msg::RESULT_ERR_PARAM};
        SendResponse(resp, 3);
        if (ok) hubConfig.Save();
    }
}

void HubUsbComm::HandleConfigGetAll(const uint8_t* payload, uint8_t len)
{
    if (len < 1) return;
    uint8_t target = payload[0];

    if (target == Msg::TARGET_KEYBOARD) {
        hubUart.SendConfigGetAll();
    }
    // Hub get-all: iterate all hub params
}

void HubUsbComm::HandleStatusReq(const uint8_t* payload, uint8_t len)
{
    if (len < 1) return;
    uint8_t target = payload[0];

    if (target == Msg::TARGET_KEYBOARD) {
        hubUart.SendStatusReq();
    } else {
        IApp* primary = appManager.GetPrimaryApp();
        IApp* einkApp = appManager.GetEinkApp();
        uint8_t resp[10] = {
            Msg::HUB_PC_STATUS, Msg::TARGET_HUB,
            (uint8_t)(CONFIG_FW_VERSION * 10),
            primary ? primary->GetId() : (uint8_t)0,
            einkApp ? einkApp->GetId() : (uint8_t)0,
            hubUart.IsKeyboardConnected() ? (uint8_t)1 : (uint8_t)0,
            hubUart.IsFnPressed() ? (uint8_t)1 : (uint8_t)0,
            appManager.GetVisibleAppCount(),
            0, 0
        };
        SendResponse(resp, 10);
    }
}

void HubUsbComm::HandleDataFeed(const uint8_t* payload, uint8_t len)
{
    if (len < 2) return;
    uint8_t feedId = payload[0];
    appManager.OnPcData(feedId, payload + 1, len - 1);
}

void HubUsbComm::HandleEinkImage(const uint8_t* payload, uint16_t len)
{
    if (len < 3) return;
    // Dispatch to AppEinkImage via PC data feed mechanism
    appManager.OnPcData(0x10, payload, (uint8_t)(len > 255 ? 255 : len));
}

void HubUsbComm::HandleEinkText(const uint8_t* payload, uint16_t len)
{
    if (len < 1) return;
    uint8_t n = len > 255 ? (uint8_t)255 : (uint8_t)len;
    appManager.OnPcData(0x30, payload, n);
}

void HubUsbComm::HandleFwInfoReq()
{
    uint8_t resp[16] = {
        Msg::HUB_PC_FW_INFO,
        1,
        (uint8_t)(CONFIG_FW_VERSION * 10),
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    SendResponse(resp, 16);
}

void HubUsbComm::HandleProfileList()
{
    uint8_t buf[200];
    buf[0] = Msg::HUB_PC_PROFILE_LIST;
    uint8_t listLen = profileStore.GetProfileList(buf + 1, sizeof(buf) - 1);
    SendResponse(buf, 1 + listLen);
}

void HubUsbComm::HandleProfileSave(const uint8_t* payload, uint16_t len)
{
    if (len < 2) return;
    uint8_t slot = payload[0];
    char name[17]{};
    uint8_t nameLen = len - 1;
    if (nameLen > 16) nameLen = 16;
    for (uint8_t i = 0; i < nameLen; i++) name[i] = (char)payload[1 + i];

    bool ok = profileStore.SaveProfile(slot, name);
    uint8_t resp[3] = {Msg::HUB_PC_ACK, Msg::PC_HUB_PROFILE_SAVE,
                       ok ? Msg::RESULT_OK : Msg::RESULT_ERR_PARAM};
    SendResponse(resp, 3);
}

void HubUsbComm::HandleProfileLoad(const uint8_t* payload, uint8_t len)
{
    if (len < 1) return;
    bool ok = profileStore.LoadProfile(payload[0]);
    uint8_t resp[3] = {Msg::HUB_PC_ACK, Msg::PC_HUB_PROFILE_LOAD,
                       ok ? Msg::RESULT_OK : Msg::RESULT_ERR_PARAM};
    SendResponse(resp, 3);
}

void HubUsbComm::HandleProfileDelete(const uint8_t* payload, uint8_t len)
{
    if (len < 1) return;
    bool ok = profileStore.DeleteProfile(payload[0]);
    uint8_t resp[3] = {Msg::HUB_PC_ACK, Msg::PC_HUB_PROFILE_DELETE,
                       ok ? Msg::RESULT_OK : Msg::RESULT_ERR_PARAM};
    SendResponse(resp, 3);
}

void HubUsbComm::HandleAppSwitch(const uint8_t* payload, uint8_t len)
{
    if (len < 1) return;
    appManager.SetPrimaryApp(payload[0]);
    uint8_t resp[3] = {Msg::HUB_PC_ACK, Msg::PC_HUB_APP_SWITCH, Msg::RESULT_OK};
    SendResponse(resp, 3);
}

void HubUsbComm::HandleEinkSwitch(const uint8_t* payload, uint8_t len)
{
    if (len < 1) return;
    appManager.SetEinkApp(payload[0]);
    uint8_t resp[3] = {Msg::HUB_PC_ACK, Msg::PC_HUB_EINK_SWITCH, Msg::RESULT_OK};
    SendResponse(resp, 3);
}
