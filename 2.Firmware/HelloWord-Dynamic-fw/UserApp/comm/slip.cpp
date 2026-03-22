#include "slip.h"

uint16_t SlipEncoder::Encode(const uint8_t* data, uint16_t len,
                             uint8_t* outBuf, uint16_t outBufSize)
{
    uint16_t pos = 0;
    if (pos >= outBufSize) return 0;
    outBuf[pos++] = SLIP_END;
    for (uint16_t i = 0; i < len; i++) {
        if (data[i] == SLIP_END) {
            if (pos + 2 > outBufSize) return 0;
            outBuf[pos++] = SLIP_ESC;
            outBuf[pos++] = SLIP_ESC_END;
        } else if (data[i] == SLIP_ESC) {
            if (pos + 2 > outBufSize) return 0;
            outBuf[pos++] = SLIP_ESC;
            outBuf[pos++] = SLIP_ESC_ESC;
        } else {
            if (pos + 1 > outBufSize) return 0;
            outBuf[pos++] = data[i];
        }
    }
    if (pos >= outBufSize) return 0;
    outBuf[pos++] = SLIP_END;
    return pos;
}

void SlipDecoder::Reset()
{
    writePos_ = 0;
    escaping_ = false;
    frameLen_ = 0;
    frameReady_ = false;
}

bool SlipDecoder::Feed(uint8_t byte)
{
    if (frameReady_) Reset();
    if (byte == SLIP_END) {
        if (writePos_ > 0) {
            frameLen_ = writePos_;
            frameReady_ = true;
            return true;
        }
        writePos_ = 0;
        return false;
    }
    if (byte == SLIP_ESC) { escaping_ = true; return false; }
    if (escaping_) {
        escaping_ = false;
        if (byte == SLIP_ESC_END) byte = SLIP_END;
        else if (byte == SLIP_ESC_ESC) byte = SLIP_ESC;
    }
    if (writePos_ < MAX_FRAME_SIZE) frameBuf_[writePos_++] = byte;
    else writePos_ = 0;
    return false;
}
