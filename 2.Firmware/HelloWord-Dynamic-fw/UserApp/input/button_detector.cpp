#include "button_detector.h"

void ButtonDetector::Update(bool pressed, uint32_t nowMs)
{
    switch (state_) {
        case IDLE:
            if (pressed) {
                state_ = PRESSED;
                pressMs_ = nowMs;
            }
            break;

        case PRESSED:
            if (!pressed) {
                if (nowMs - pressMs_ >= LONG_PRESS_MS) {
                    pending_ = BTN_LONG_PRESS;
                    state_ = IDLE;
                } else {
                    state_ = RELEASED_ONCE;
                    releaseMs_ = nowMs;
                }
            } else if (nowMs - pressMs_ >= LONG_PRESS_MS) {
                pending_ = BTN_LONG_PRESS;
                state_ = IDLE;
            }
            break;

        case RELEASED_ONCE:
            if (pressed) {
                state_ = PRESSED_SECOND;
                pressMs_ = nowMs;
            } else if (nowMs - releaseMs_ >= DOUBLE_CLICK_WINDOW_MS) {
                pending_ = BTN_SHORT_PRESS;
                state_ = IDLE;
            }
            break;

        case PRESSED_SECOND:
            if (!pressed) {
                pending_ = BTN_DOUBLE_CLICK;
                state_ = IDLE;
            } else if (nowMs - pressMs_ >= LONG_PRESS_MS) {
                pending_ = BTN_LONG_PRESS;
                state_ = IDLE;
            }
            break;
    }
}

ButtonEvent ButtonDetector::Poll()
{
    ButtonEvent e = pending_;
    pending_ = BTN_NONE;
    return e;
}
