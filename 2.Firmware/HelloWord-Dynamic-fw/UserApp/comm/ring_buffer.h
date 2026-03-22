#ifndef RING_BUFFER_H
#define RING_BUFFER_H

// Shared ring buffer — identical to keyboard-fw version
#include <stdint.h>

template<uint16_t SIZE>
class RingBuffer {
public:
    bool Write(uint8_t byte) {
        uint16_t next = (head_ + 1) % SIZE;
        if (next == tail_) return false;
        buf_[head_] = byte;
        head_ = next;
        return true;
    }
    bool Write(const uint8_t* data, uint16_t len) {
        for (uint16_t i = 0; i < len; i++)
            if (!Write(data[i])) return false;
        return true;
    }
    bool Read(uint8_t& byte) {
        if (tail_ == head_) return false;
        byte = buf_[tail_];
        tail_ = (tail_ + 1) % SIZE;
        return true;
    }
    uint16_t Available() const {
        if (head_ >= tail_) return head_ - tail_;
        return SIZE - tail_ + head_;
    }
    bool IsEmpty() const { return head_ == tail_; }
    bool IsFull() const { return ((head_ + 1) % SIZE) == tail_; }
    void Clear() { head_ = tail_ = 0; }
private:
    volatile uint16_t head_ = 0;
    volatile uint16_t tail_ = 0;
    uint8_t buf_[SIZE]{};
};

#endif // RING_BUFFER_H
