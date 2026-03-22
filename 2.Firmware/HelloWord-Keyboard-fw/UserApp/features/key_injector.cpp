#include "key_injector.h"
#include "comm/protocol.h"

KeyInjector keyInjector;

bool KeyInjector::Enqueue(uint8_t action, uint8_t modifier, uint8_t keycode, uint8_t holdFrames)
{
    uint8_t next = (head_ + 1) % QUEUE_SIZE;
    if (next == tail_) return false;

    queue_[head_] = {action, modifier, keycode, holdFrames};
    head_ = next;
    return true;
}

void KeyInjector::ProcessFrame(HWKeyboard& kb)
{
    // Process active hold
    if (activeFrames_ > 0) {
        // Apply modifier bits
        if (activeModifiers_ & 0x01) kb.Press(HWKeyboard::LEFT_CTRL);
        if (activeModifiers_ & 0x02) kb.Press(HWKeyboard::LEFT_SHIFT);
        if (activeModifiers_ & 0x04) kb.Press(HWKeyboard::LEFT_ALT);
        if (activeModifiers_ & 0x08) kb.Press(HWKeyboard::LEFT_GUI);
        if (activeModifiers_ & 0x10) kb.Press(HWKeyboard::RIGHT_CTRL);
        if (activeModifiers_ & 0x20) kb.Press(HWKeyboard::RIGHT_SHIFT);
        if (activeModifiers_ & 0x40) kb.Press(HWKeyboard::RIGHT_ALT);
        if (activeModifiers_ & 0x80) kb.Press(HWKeyboard::RIGHT_GUI);

        if (activeKeycode_ > 0 && activeKeycode_ < 128)
            kb.Press((HWKeyboard::KeyCode_t)activeKeycode_);

        activeFrames_--;
        if (activeFrames_ == 0) {
            activeModifiers_ = 0;
            activeKeycode_ = 0;
        }
        return;
    }

    // Dequeue next entry
    if (tail_ == head_) return;

    Entry& e = queue_[tail_];
    tail_ = (tail_ + 1) % QUEUE_SIZE;

    switch (e.action) {
        case Msg::KEY_ACTION_TAP:
        case Msg::KEY_ACTION_PRESS:
            activeModifiers_ = e.modifier;
            activeKeycode_ = e.keycode;
            activeFrames_ = e.framesLeft > 0 ? e.framesLeft : 2;
            // Apply immediately this frame
            ProcessFrame(kb);
            break;

        case Msg::KEY_ACTION_RELEASE:
            activeModifiers_ = 0;
            activeKeycode_ = 0;
            activeFrames_ = 0;
            break;

        default:
            break;
    }
}
