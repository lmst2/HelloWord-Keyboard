#include "input_manager.h"

InputManager inputManager;

void InputManager::Update(bool keyAPressed, bool keyBPressed, uint32_t nowMs)
{
    btnA_.Update(keyAPressed, nowMs);
    btnB_.Update(keyBPressed, nowMs);

    ButtonEvent evA = btnA_.Poll();
    ButtonEvent evB = btnB_.Poll();

    if (evA != BTN_NONE || evB != BTN_NONE)
        DispatchEvent(evA, evB);
}

void InputManager::DispatchEvent(ButtonEvent evA, ButtonEvent evB)
{
    if (!mgr_) return;

    // KEY_A events
    switch (evA) {
        case BTN_SHORT_PRESS:
            mgr_->NextPrimaryApp();
            break;
        case BTN_LONG_PRESS:
            if (mgr_->IsInSubmenu())
                ; // reserved for L3 / confirm
            else
                mgr_->EnterSubmenu();
            break;
        case BTN_DOUBLE_CLICK:
            mgr_->PrevEinkApp();
            break;
        default:
            break;
    }

    // KEY_B events
    switch (evB) {
        case BTN_SHORT_PRESS:
            mgr_->PrevPrimaryApp();
            break;
        case BTN_LONG_PRESS:
            mgr_->ExitSubmenu();
            break;
        case BTN_DOUBLE_CLICK:
            mgr_->NextEinkApp();
            break;
        default:
            break;
    }
}
