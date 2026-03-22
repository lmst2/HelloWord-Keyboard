#include "hub_uart_comm.h"
#include "protocol.h"
#include "config_cache.h"
#include "common_inc.h"
#include "usart.h"

HubUartComm hubUart;
extern ConfigCache kbConfigCache;

static void UartSendRaw(const uint8_t* data, uint16_t len)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)data, len, 50);
}

void HubUartComm::Init()
{
    decoder_.Reset();
}

void HubUartComm::ProcessBytes(const uint8_t* data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        if (decoder_.Feed(data[i])) {
            HandleFrame(decoder_.GetFrame(), decoder_.GetFrameLen());
        }
    }
}

bool HubUartComm::Send(uint8_t cmd, const uint8_t* payload, uint8_t payloadLen)
{
    uint8_t frameBuf[SlipDecoder::MAX_FRAME_SIZE];
    if (1 + payloadLen > SlipDecoder::MAX_FRAME_SIZE) return false;

    frameBuf[0] = cmd;
    if (payload && payloadLen > 0) {
        for (uint8_t i = 0; i < payloadLen; i++)
            frameBuf[1 + i] = payload[i];
    }

    uint16_t encoded = SlipEncoder::Encode(frameBuf, 1 + payloadLen, txBuf_, sizeof(txBuf_));
    if (encoded == 0) return false;

    UartSendRaw(txBuf_, encoded);
    return true;
}

bool HubUartComm::SendKeyAction(uint8_t action, uint8_t modifier, uint8_t keycode, uint8_t holdFrames)
{
    uint8_t payload[4] = {action, modifier, keycode, holdFrames};
    return Send(Msg::HUB_KB_KEY_ACTION, payload, 4);
}

bool HubUartComm::SendConfigGet(uint16_t paramId)
{
    uint8_t payload[2] = {(uint8_t)(paramId >> 8), (uint8_t)(paramId & 0xFF)};
    return Send(Msg::HUB_KB_CONFIG_GET, payload, 2);
}

bool HubUartComm::SendConfigSet(uint16_t paramId, const uint8_t* value, uint8_t len)
{
    uint8_t payload[6];
    payload[0] = (uint8_t)(paramId >> 8);
    payload[1] = (uint8_t)(paramId & 0xFF);
    for (uint8_t i = 0; i < len && i < 4; i++) payload[2 + i] = value[i];
    return Send(Msg::HUB_KB_CONFIG_SET, payload, 2 + len);
}

bool HubUartComm::SendConfigGetAll()
{
    return Send(Msg::HUB_KB_CONFIG_GET_ALL);
}

bool HubUartComm::SendStatusReq()
{
    return Send(Msg::HUB_KB_STATUS_REQ);
}

void HubUartComm::HandleFrame(const uint8_t* data, uint16_t len)
{
    if (len < 1) return;

    uint8_t cmd = data[0];
    const uint8_t* payload = data + 1;
    uint8_t payloadLen = (uint8_t)(len - 1);

    switch (cmd) {
        case Msg::KB_HUB_FN_STATE:
            if (payloadLen >= 1) fnPressed_ = payload[0] != 0;
            kbConnected_ = true;
            break;

        case Msg::KB_HUB_CONFIG_VALUE:
            if (payloadLen >= 3) {
                uint16_t paramId = ((uint16_t)payload[0] << 8) | payload[1];
                kbConfigCache.UpdateParam(paramId, payload + 2, payloadLen - 2);
            }
            kbConnected_ = true;
            break;

        case Msg::KB_HUB_CONFIG_ACK:
            kbConnected_ = true;
            break;

        case Msg::KB_HUB_STATUS:
            if (payloadLen >= 6) {
                kbConfigCache.UpdateFromStatus(payload, payloadLen);
            }
            kbConnected_ = true;
            break;

        case Msg::KB_HUB_PING:
            kbConnected_ = true;
            lastPingMs_ = HAL_GetTick();
            break;

        case Msg::KB_HUB_KEY_EVENT:
        case Msg::KB_HUB_TOUCHBAR:
            kbConnected_ = true;
            break;

        default:
            break;
    }
}
