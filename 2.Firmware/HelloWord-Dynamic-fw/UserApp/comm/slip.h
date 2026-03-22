#ifndef SLIP_H
#define SLIP_H

// Shared SLIP — identical to keyboard-fw version
#include <stdint.h>

static constexpr uint8_t SLIP_END     = 0xC0;
static constexpr uint8_t SLIP_ESC     = 0xDB;
static constexpr uint8_t SLIP_ESC_END = 0xDC;
static constexpr uint8_t SLIP_ESC_ESC = 0xDD;

class SlipEncoder {
public:
    static uint16_t Encode(const uint8_t* data, uint16_t len, uint8_t* outBuf, uint16_t outBufSize);
};

class SlipDecoder {
public:
    bool Feed(uint8_t byte);
    const uint8_t* GetFrame() const { return frameBuf_; }
    uint16_t GetFrameLen() const { return frameLen_; }
    void Reset();
    static constexpr uint16_t MAX_FRAME_SIZE = 128;
private:
    uint8_t frameBuf_[MAX_FRAME_SIZE]{};
    uint16_t writePos_ = 0;
    bool escaping_ = false;
    uint16_t frameLen_ = 0;
    bool frameReady_ = false;
};

#endif // SLIP_H
