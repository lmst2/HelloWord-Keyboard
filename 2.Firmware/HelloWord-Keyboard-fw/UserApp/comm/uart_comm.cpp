#include "uart_comm.h"
#include "protocol.h"
#include "config_handler.h"
#include "common_inc.h"
#include "../features/key_injector.h"

extern ConfigHandler configHandler;
extern KeyInjector keyInjector;

UartComm uartComm;

extern "C" void UartComm_OnByteISR(uint8_t b)
{
    uartComm.OnByteReceived(b);
}

static void UartSendRaw(const uint8_t* data, uint16_t len)
{
    HAL_UART_Transmit(&huart1, (uint8_t*)data, len, 50);
}

void UartComm::Init()
{
    decoder_.Reset();
    rxRing_.Clear();

    // Enable USART1 RXNE interrupt
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
}

void UartComm::OnByteReceived(uint8_t byte)
{
    rxRing_.Write(byte);
}

void UartComm::Poll()
{
    uint8_t byte;
    while (rxRing_.Read(byte)) {
        if (decoder_.Feed(byte)) {
            HandleFrame(decoder_.GetFrame(), decoder_.GetFrameLen());
        }
    }
}

bool UartComm::Send(uint8_t cmd, const uint8_t* payload, uint8_t payloadLen)
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

void UartComm::HandleFrame(const uint8_t* data, uint16_t len)
{
    if (len < 1) return;

    uint8_t cmd = data[0];
    const uint8_t* payload = data + 1;
    uint8_t payloadLen = (uint8_t)(len - 1);

    switch (cmd) {
        case Msg::HUB_KB_CONFIG_GET: {
            if (payloadLen < 2) break;
            uint16_t paramId = ((uint16_t)payload[0] << 8) | payload[1];
            uint8_t val[4];
            uint8_t vlen = configHandler.GetParam(paramId, val);
            if (vlen > 0) {
                uint8_t resp[6] = {(uint8_t)(paramId >> 8), (uint8_t)(paramId & 0xFF)};
                for (uint8_t i = 0; i < vlen; i++) resp[2 + i] = val[i];
                Send(Msg::KB_HUB_CONFIG_VALUE, resp, 2 + vlen);
            }
            break;
        }

        case Msg::HUB_KB_CONFIG_SET: {
            if (payloadLen < 3) break;
            uint16_t paramId = ((uint16_t)payload[0] << 8) | payload[1];
            bool ok = configHandler.SetParam(paramId, payload + 2, payloadLen - 2);
            uint8_t ack[3] = {(uint8_t)(paramId >> 8), (uint8_t)(paramId & 0xFF),
                              ok ? Msg::RESULT_OK : Msg::RESULT_ERR_PARAM};
            Send(Msg::KB_HUB_CONFIG_ACK, ack, 3);
            if (ok) configHandler.Save();
            break;
        }

        case Msg::HUB_KB_CONFIG_GET_ALL: {
            uint8_t buf[128];
            uint16_t total = configHandler.GetAllParams(buf, sizeof(buf));
            if (total > 0)
                Send(Msg::KB_HUB_CONFIG_VALUE, buf, (uint8_t)total);
            break;
        }

        case Msg::HUB_KB_STATUS_REQ: {
            extern KeyboardConfig_t config;
            uint8_t status[8] = {
                1, // firmware version
                config.effectMode,
                config.brightness,
                config.activeLayer,
                config.osMode,
                config.touchBar.mode,
                0, // reserved
                0
            };
            Send(Msg::KB_HUB_STATUS, status, sizeof(status));
            break;
        }

        case Msg::HUB_KB_KEY_ACTION: {
            if (payloadLen < 4) break;
            keyInjector.Enqueue(payload[0], payload[1], payload[2], payload[3]);
            break;
        }

        case Msg::HUB_KB_RGB_MODE: {
            if (payloadLen < 2) break;
            extern HWKeyboard keyboard;
            keyboard.SetEffect((HWKeyboard::LightEffect_t)payload[1]);
            break;
        }

        case Msg::HUB_KB_DFU: {
            RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
            PWR->CR |= PWR_CR_DBP;
            BKP->DR1 = 0xB011U;
            NVIC_SystemReset();
            break;
        }

        default:
            break;
    }
}
