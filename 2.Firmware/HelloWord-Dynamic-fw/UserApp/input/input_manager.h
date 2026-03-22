#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include "button_detector.h"
#include "app/app_manager.h"

class InputManager {
public:
    void Init(AppManager* mgr) { mgr_ = mgr; }

    // Called each display tick (~20ms) with raw GPIO states
    void Update(bool keyAPressed, bool keyBPressed, uint32_t nowMs);

private:
    void DispatchEvent(ButtonEvent evA, ButtonEvent evB);

    AppManager* mgr_ = nullptr;
    ButtonDetector btnA_;
    ButtonDetector btnB_;
};

extern InputManager inputManager;

#endif // INPUT_MANAGER_H
