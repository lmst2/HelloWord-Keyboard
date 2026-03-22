#ifndef BUTTON_DETECTOR_H
#define BUTTON_DETECTOR_H

#include <stdint.h>

enum ButtonEvent : uint8_t {
    BTN_NONE = 0,
    BTN_SHORT_PRESS,
    BTN_LONG_PRESS,
    BTN_DOUBLE_CLICK,
};

class ButtonDetector {
public:
    void Update(bool pressed, uint32_t nowMs);
    ButtonEvent Poll();

private:
    enum State { IDLE, PRESSED, RELEASED_ONCE, PRESSED_SECOND };
    State state_ = IDLE;
    uint32_t pressMs_ = 0;
    uint32_t releaseMs_ = 0;
    ButtonEvent pending_ = BTN_NONE;

    static constexpr uint32_t LONG_PRESS_MS = 500;
    static constexpr uint32_t DOUBLE_CLICK_WINDOW_MS = 300;
    static constexpr uint32_t SHORT_PRESS_MAX_MS = 300;
};

#endif // BUTTON_DETECTOR_H
