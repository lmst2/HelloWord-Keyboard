#ifndef UART_COMM_H
#define UART_COMM_H

#include <stdint.h>
#include "slip.h"
#include "ring_buffer.h"

class UartComm {
public:
    void Init();

    // Called from USART1 RXNE interrupt — single byte at a time
    void OnByteReceived(uint8_t byte);

    // Called from main loop to process decoded frames
    void Poll();

    // Send a SLIP-framed message: [cmd][payload...]
    bool Send(uint8_t cmd, const uint8_t* payload = nullptr, uint8_t payloadLen = 0);

private:
    void HandleFrame(const uint8_t* data, uint16_t len);

    RingBuffer<256> rxRing_;
    SlipDecoder decoder_;
    uint8_t txBuf_[280]{};
};

extern UartComm uartComm;

#endif // UART_COMM_H
