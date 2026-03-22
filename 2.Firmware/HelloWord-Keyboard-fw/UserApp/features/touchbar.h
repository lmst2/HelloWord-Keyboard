#ifndef TOUCHBAR_H
#define TOUCHBAR_H

#include <stdint.h>
#include "HelloWord/hw_keyboard.h"
#include "configurations.h"

enum TouchBarMode_t : uint8_t
{
    TOUCHBAR_MODE_PAN = 0,
    TOUCHBAR_MODE_APP_SWITCH,
    TOUCHBAR_MODE_DESKTOP_SWITCH,
    TOUCHBAR_MODE_COUNT
};

class TouchBarProcessor {
public:
    void Init(const TouchBarConfig_t* cfg) { cfg_ = cfg; }

    // Called from 1kHz timer callback — processes touch state and queues synthetic keys
    void Process(uint32_t nowMs, HWKeyboard& kb);

    // Force clear all touchbar state (called when Fn is held)
    void ClearActions();

    void CycleMode();
    TouchBarMode_t GetMode() const { return mode_; }
    void SetMode(TouchBarMode_t m) { mode_ = m; }

    // Status blink support (returns flash count to trigger, 0 = none)
    uint8_t ConsumePendingBlink();

private:
    struct Session {
        bool isTouching = false;
        bool isGestureActive = false;
        bool isDesktopSeekMode = false;
        bool isNoTouchPending = false;
        uint8_t activeSegment = 0xFF;
        uint8_t activeTouchCount = 0;
        uint32_t touchStartMs = 0;
        uint32_t lastTouchMs = 0;
        uint32_t lastPanMs = 0;
        uint32_t lastStepMs = 0;
        uint32_t edgeHoldStartMs = 0;
        uint32_t appSwitchReleaseGuardUntilMs = 0;
        int16_t anchorPosition = 0;
        int16_t currentPosition = 0;
        int16_t emittedSteps = 0;
        int8_t edgeHoldDirection = 0;
    };

    struct SyntheticKeys {
        bool holdLeftAlt = false;
        bool holdLeftShift = false;
        bool hasShiftScrollPrimed = false;
        uint8_t leftShiftFrames = 0;
        uint8_t leftCtrlFrames = 0;
        uint8_t leftGuiFrames = 0;
        uint8_t tabDelayFrames = 0;
        uint8_t tabFrames = 0;
        uint8_t leftArrowDelayFrames = 0;
        uint8_t rightArrowDelayFrames = 0;
        uint8_t leftArrowFrames = 0;
        uint8_t rightArrowFrames = 0;
    };

    void HandlePanMode(uint32_t nowMs, HWKeyboard& kb);
    void HandleAppSwitchMode(uint32_t nowMs);
    void HandleDesktopSwitchMode(uint32_t nowMs);
    void FinalizeDesktopSwitchGesture();

    void QueueMouseWheel(int8_t wheel, HWKeyboard& kb);
    void QueueAppSwitchStep(int16_t direction);
    void QueueDesktopSwitchStep(int16_t direction);

    void ResetEdgeHold();
    void ArmEdgeHold(uint32_t nowMs, int16_t dir);
    int16_t GetEdgeDirection() const;
    bool TryRepeatStepAtEdge(uint32_t nowMs, uint32_t holdDelayMs, uint32_t stepIntervalMs,
                             int16_t stepDistance, void (TouchBarProcessor::*queueStep)(int16_t));
    bool ShouldDelayDesktopEdgeContinuation(uint32_t nowMs, int16_t targetSteps);

    uint32_t GetActivationDelayMs() const;
    uint32_t GetReleaseGraceMs() const;

    void ApplySyntheticKeys(HWKeyboard& kb);

    // TouchBar position helpers
    static uint8_t SelectSegment(uint8_t touchState);
    static int16_t GetSegmentPosition(uint8_t touchState, uint8_t seg);
    static uint8_t CountSegmentTouches(uint8_t touchState, uint8_t seg);

    TouchBarMode_t mode_ = TOUCHBAR_MODE_PAN;
    Session s_{};
    SyntheticKeys sk_{};
    const TouchBarConfig_t* cfg_ = nullptr;
    uint8_t pendingBlink_ = 0;

    // Mouse report staging — accessed by free functions below
    uint8_t pendingMouseReport_[HWKeyboard::MOUSE_REPORT_SIZE]{};
    bool hasPendingMouse_ = false;

    friend bool TouchBar_HasPendingMouseReport();
    friend bool TouchBar_TrySendMouseReport();

    static constexpr uint8_t SEGMENT_COUNT = 2;
    static constexpr uint8_t TOUCHES_PER_SEGMENT = 4;
    static constexpr uint8_t ENTRY_TOUCHES_PER_SEGMENT = 3;
    static constexpr uint8_t INVALID_SEGMENT = 0xFF;
    static constexpr int16_t POSITION_SCALE = 256;
    static constexpr int16_t PAN_DEADZONE = 64;
    static constexpr int16_t STEP_DISTANCE = 160;
    static constexpr int16_t DESKTOP_STEP_DISTANCE = 256;
    static constexpr int16_t DESKTOP_SWIPE_DISTANCE = 96;
    static constexpr int16_t EDGE_REPEAT_THRESHOLD = 64;
    static constexpr uint8_t SYNTHETIC_PULSE_FRAMES = 2;

    static constexpr uint32_t DEFAULT_ACTIVATION_MS = 20;
    static constexpr uint32_t APP_ACTIVATION_MS = 90;
    static constexpr uint32_t DESKTOP_HOLD_MS = 500;
    static constexpr uint32_t APP_EDGE_REPEAT_DELAY_MS = 400;
    static constexpr uint32_t DESKTOP_EDGE_REPEAT_DELAY_MS = 1200;
    static constexpr uint32_t APP_RELEASE_SETTLE_MS = 50;
    static constexpr uint32_t DEFAULT_RELEASE_GRACE_MS = 35;
    static constexpr uint32_t SWITCH_RELEASE_GRACE_MS = 90;
    static constexpr uint32_t PAN_INTERVAL_MS = 12;
    static constexpr uint32_t APP_STEP_INTERVAL_MS = 55;
    static constexpr uint32_t DESKTOP_STEP_INTERVAL_MS = 500;
};

extern TouchBarProcessor touchBar;

// Access to pending mouse report for SendPendingReports()
bool TouchBar_HasPendingMouseReport();
bool TouchBar_TrySendMouseReport();

#endif // TOUCHBAR_H
