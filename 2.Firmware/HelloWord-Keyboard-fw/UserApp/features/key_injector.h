#ifndef KEY_INJECTOR_H
#define KEY_INJECTOR_H

#include <stdint.h>
#include "HelloWord/hw_keyboard.h"

class KeyInjector {
public:
    // Called from UART handler (ISR-safe: just enqueue)
    bool Enqueue(uint8_t action, uint8_t modifier, uint8_t keycode, uint8_t holdFrames);

    // Called from OnTimerCallback at 1kHz — applies injected keys to HID report
    void ProcessFrame(HWKeyboard& kb);

    bool HasPending() const { return head_ != tail_; }

private:
    static constexpr uint8_t QUEUE_SIZE = 8;

    struct Entry {
        uint8_t action;
        uint8_t modifier;
        uint8_t keycode;
        uint8_t framesLeft;
    };

    Entry queue_[QUEUE_SIZE]{};
    volatile uint8_t head_ = 0;
    volatile uint8_t tail_ = 0;

    // Currently held injected modifiers and key
    uint8_t activeModifiers_ = 0;
    uint8_t activeKeycode_ = 0;
    uint8_t activeFrames_ = 0;
};

extern KeyInjector keyInjector;

#endif // KEY_INJECTOR_H
