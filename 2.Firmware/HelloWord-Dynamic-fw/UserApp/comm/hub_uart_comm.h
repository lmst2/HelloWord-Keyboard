#ifndef HUB_UART_COMM_H
#define HUB_UART_COMM_H

#include <stdint.h>
#include "slip.h"

class HubUartComm {
public:
    void Init();

    // Called from UART DMA task to process received bytes
    void ProcessBytes(const uint8_t* data, uint16_t len);

    // Send a SLIP-framed message to keyboard
    bool Send(uint8_t cmd, const uint8_t* payload = nullptr, uint8_t payloadLen = 0);

    // Send key action to keyboard (convenience wrapper)
    bool SendKeyAction(uint8_t action, uint8_t modifier, uint8_t keycode, uint8_t holdFrames);

    // Send config get/set to keyboard
    bool SendConfigGet(uint16_t paramId);
    bool SendConfigSet(uint16_t paramId, const uint8_t* value, uint8_t len);
    bool SendConfigGetAll();
    bool SendStatusReq();

    // Keyboard connection state
    bool IsKeyboardConnected() const { return kbConnected_; }
    bool IsFnPressed() const { return fnPressed_; }

private:
    void HandleFrame(const uint8_t* data, uint16_t len);

    SlipDecoder decoder_;
    uint8_t txBuf_[280]{};
    bool kbConnected_ = false;
    bool fnPressed_ = false;
    uint32_t lastPingMs_ = 0;
};

extern HubUartComm hubUart;

#endif // HUB_UART_COMM_H
