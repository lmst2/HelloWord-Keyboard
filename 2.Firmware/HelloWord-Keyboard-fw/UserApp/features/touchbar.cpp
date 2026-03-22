#include "touchbar.h"
#include "common_inc.h"
#include <cstring>

TouchBarProcessor touchBar;

static const uint8_t SEGMENT_TOUCH_MAP[2][4] = {{0,1,2,3}, {2,3,4,5}};
static const uint8_t SEGMENT_ENTRY_MAP[2][3] = {{0,1,2}, {3,4,5}};

static uint8_t GetTouchBit(uint8_t logicalPos)
{
    return (uint8_t)(1U << (HWKeyboard::TOUCHPAD_NUMBER - 1U - logicalPos));
}

static uint8_t CountMappedTouches(uint8_t touchState, const uint8_t* map, uint8_t count)
{
    uint8_t n = 0;
    for (uint8_t i = 0; i < count; i++)
        if (touchState & GetTouchBit(map[i])) n++;
    return n;
}

static int16_t GetMappedPosition(uint8_t touchState, const uint8_t* map, uint8_t count)
{
    uint16_t sum = 0;
    uint8_t active = 0;
    for (uint8_t i = 0; i < count; i++) {
        if (touchState & GetTouchBit(map[i])) {
            sum += (uint16_t)i * 256;
            active++;
        }
    }
    return active == 0 ? -1 : (int16_t)(sum / active);
}

static inline int16_t Abs16(int16_t v) { return v >= 0 ? v : (int16_t)-v; }

uint8_t TouchBarProcessor::SelectSegment(uint8_t touchState)
{
    uint8_t left = CountMappedTouches(touchState, SEGMENT_ENTRY_MAP[0], ENTRY_TOUCHES_PER_SEGMENT);
    uint8_t right = CountMappedTouches(touchState, SEGMENT_ENTRY_MAP[1], ENTRY_TOUCHES_PER_SEGMENT);
    if (left == 0 && right == 0) return INVALID_SEGMENT;
    if (left > right) return 0;
    if (right > left) return 1;
    static const uint8_t globalMap[6] = {0,1,2,3,4,5};
    int16_t gpos = GetMappedPosition(touchState, globalMap, 6);
    if (gpos < 0) return INVALID_SEGMENT;
    return gpos < (3 * POSITION_SCALE) ? 0 : 1;
}

int16_t TouchBarProcessor::GetSegmentPosition(uint8_t touchState, uint8_t seg)
{
    return GetMappedPosition(touchState, SEGMENT_TOUCH_MAP[seg], TOUCHES_PER_SEGMENT);
}

uint8_t TouchBarProcessor::CountSegmentTouches(uint8_t touchState, uint8_t seg)
{
    return CountMappedTouches(touchState, SEGMENT_TOUCH_MAP[seg], TOUCHES_PER_SEGMENT);
}

uint32_t TouchBarProcessor::GetActivationDelayMs() const
{
    if (mode_ == TOUCHBAR_MODE_APP_SWITCH) return APP_ACTIVATION_MS;
    return cfg_ ? cfg_->activationMs : DEFAULT_ACTIVATION_MS;
}

uint32_t TouchBarProcessor::GetReleaseGraceMs() const
{
    switch (mode_) {
        case TOUCHBAR_MODE_APP_SWITCH:
        case TOUCHBAR_MODE_DESKTOP_SWITCH:
            return SWITCH_RELEASE_GRACE_MS;
        default:
            return cfg_ ? cfg_->releaseGraceMs : DEFAULT_RELEASE_GRACE_MS;
    }
}

uint8_t TouchBarProcessor::ConsumePendingBlink()
{
    uint8_t b = pendingBlink_;
    pendingBlink_ = 0;
    return b;
}

void TouchBarProcessor::ClearActions()
{
    s_ = Session{};
    sk_ = SyntheticKeys{};
}

void TouchBarProcessor::CycleMode()
{
    mode_ = (TouchBarMode_t)((mode_ + 1) % TOUCHBAR_MODE_COUNT);
    pendingBlink_ = (uint8_t)mode_ + 1;
}

void TouchBarProcessor::QueueMouseWheel(int8_t wheel, HWKeyboard& kb)
{
    if (wheel == 0) return;
    kb.SetMouseWheel(wheel);
    memcpy(pendingMouseReport_, kb.GetHidReportBuffer(3), HWKeyboard::MOUSE_REPORT_SIZE);
    hasPendingMouse_ = true;
    kb.ClearMouseReport();
}

void TouchBarProcessor::QueueAppSwitchStep(int16_t direction)
{
    sk_.holdLeftAlt = true;
    sk_.tabDelayFrames = 1;
    sk_.tabFrames = SYNTHETIC_PULSE_FRAMES;
    sk_.leftShiftFrames = direction < 0 ? SYNTHETIC_PULSE_FRAMES + 1 : 0;
}

void TouchBarProcessor::QueueDesktopSwitchStep(int16_t direction)
{
    sk_.leftCtrlFrames = SYNTHETIC_PULSE_FRAMES + 1;
    sk_.leftGuiFrames = SYNTHETIC_PULSE_FRAMES + 1;
    sk_.leftArrowDelayFrames = sk_.rightArrowDelayFrames = 0;
    sk_.leftArrowFrames = sk_.rightArrowFrames = 0;
    if (direction < 0) {
        sk_.leftArrowDelayFrames = 1;
        sk_.leftArrowFrames = SYNTHETIC_PULSE_FRAMES;
    } else {
        sk_.rightArrowDelayFrames = 1;
        sk_.rightArrowFrames = SYNTHETIC_PULSE_FRAMES;
    }
}

int16_t TouchBarProcessor::GetEdgeDirection() const
{
    int16_t maxPos = (TOUCHES_PER_SEGMENT - 1) * POSITION_SCALE;
    if (s_.currentPosition <= EDGE_REPEAT_THRESHOLD) return -1;
    if (s_.currentPosition >= maxPos - EDGE_REPEAT_THRESHOLD) return 1;
    return 0;
}

void TouchBarProcessor::ResetEdgeHold()
{
    s_.edgeHoldStartMs = 0;
    s_.edgeHoldDirection = 0;
}

void TouchBarProcessor::ArmEdgeHold(uint32_t nowMs, int16_t dir)
{
    s_.edgeHoldDirection = (int8_t)dir;
    s_.edgeHoldStartMs = nowMs;
}

bool TouchBarProcessor::TryRepeatStepAtEdge(uint32_t nowMs, uint32_t holdDelayMs,
                                             uint32_t stepIntervalMs, int16_t stepDistance,
                                             void (TouchBarProcessor::*queueStep)(int16_t))
{
    int16_t edgeDir = GetEdgeDirection();
    if (edgeDir == 0) { ResetEdgeHold(); return false; }
    if (s_.edgeHoldDirection != edgeDir) { ArmEdgeHold(nowMs, edgeDir); return true; }
    if (nowMs - s_.edgeHoldStartMs < holdDelayMs) return true;
    if (nowMs - s_.lastStepMs < stepIntervalMs) return true;
    s_.lastStepMs = nowMs;
    (this->*queueStep)(edgeDir);
    s_.emittedSteps += edgeDir;
    s_.anchorPosition -= edgeDir * stepDistance;
    return true;
}

bool TouchBarProcessor::ShouldDelayDesktopEdgeContinuation(uint32_t nowMs, int16_t targetSteps)
{
    int16_t edgeDir = GetEdgeDirection();
    if (edgeDir == 0) { ResetEdgeHold(); return false; }
    int16_t pendingDir = targetSteps > s_.emittedSteps ? 1 : -1;
    if (pendingDir != edgeDir) return false;
    if (s_.edgeHoldDirection != edgeDir) { ArmEdgeHold(nowMs, edgeDir); return false; }
    return nowMs - s_.edgeHoldStartMs < DESKTOP_EDGE_REPEAT_DELAY_MS;
}

void TouchBarProcessor::HandlePanMode(uint32_t nowMs, HWKeyboard& kb)
{
    if (nowMs - s_.lastPanMs < PAN_INTERVAL_MS) return;
    s_.lastPanMs = nowMs;
    int16_t disp = s_.currentPosition - s_.anchorPosition;
    int16_t dist = Abs16(disp);
    if (dist <= PAN_DEADZONE) return;
    int16_t speed = 1 + (dist - PAN_DEADZONE) / (POSITION_SCALE / 2);
    if (speed > 6) speed = 6;
    sk_.holdLeftShift = true;
    if (!sk_.hasShiftScrollPrimed) { sk_.hasShiftScrollPrimed = true; return; }
    QueueMouseWheel((int8_t)(disp > 0 ? -speed : speed), kb);
}

void TouchBarProcessor::HandleAppSwitchMode(uint32_t nowMs)
{
    if (nowMs < s_.appSwitchReleaseGuardUntilMs) return;
    int16_t disp = s_.currentPosition - s_.anchorPosition;
    int16_t targetSteps = disp / STEP_DISTANCE;
    if (targetSteps == s_.emittedSteps) {
        TryRepeatStepAtEdge(nowMs, APP_EDGE_REPEAT_DELAY_MS, APP_STEP_INTERVAL_MS,
                            STEP_DISTANCE, &TouchBarProcessor::QueueAppSwitchStep);
        return;
    }
    if (nowMs - s_.lastStepMs < APP_STEP_INTERVAL_MS) return;
    s_.lastStepMs = nowMs;
    s_.edgeHoldStartMs = 0;
    s_.edgeHoldDirection = 0;
    if (targetSteps > s_.emittedSteps) { QueueAppSwitchStep(1); s_.emittedSteps++; }
    else { QueueAppSwitchStep(-1); s_.emittedSteps--; }
}

void TouchBarProcessor::FinalizeDesktopSwitchGesture()
{
    if (!s_.isGestureActive || s_.isDesktopSeekMode) return;
    int16_t disp = s_.currentPosition - s_.anchorPosition;
    if (Abs16(disp) < DESKTOP_SWIPE_DISTANCE) return;
    QueueDesktopSwitchStep(disp > 0 ? 1 : -1);
}

void TouchBarProcessor::HandleDesktopSwitchMode(uint32_t nowMs)
{
    if (!s_.isDesktopSeekMode) {
        if (nowMs - s_.touchStartMs < DESKTOP_HOLD_MS) return;
        s_.isDesktopSeekMode = true;
        s_.anchorPosition = s_.currentPosition;
        s_.emittedSteps = 0;
        s_.edgeHoldStartMs = 0;
        s_.edgeHoldDirection = 0;
        s_.lastStepMs = nowMs - DESKTOP_STEP_INTERVAL_MS;
        return;
    }
    int16_t disp = s_.currentPosition - s_.anchorPosition;
    int16_t targetSteps = disp / DESKTOP_STEP_DISTANCE;
    if (targetSteps == s_.emittedSteps) {
        TryRepeatStepAtEdge(nowMs, DESKTOP_EDGE_REPEAT_DELAY_MS, DESKTOP_STEP_INTERVAL_MS,
                            DESKTOP_STEP_DISTANCE, &TouchBarProcessor::QueueDesktopSwitchStep);
        return;
    }
    if (ShouldDelayDesktopEdgeContinuation(nowMs, targetSteps)) return;
    if (nowMs - s_.lastStepMs < DESKTOP_STEP_INTERVAL_MS) return;
    s_.lastStepMs = nowMs;
    if (GetEdgeDirection() == 0) ResetEdgeHold();
    if (targetSteps > s_.emittedSteps) { QueueDesktopSwitchStep(1); s_.emittedSteps++; }
    else { QueueDesktopSwitchStep(-1); s_.emittedSteps--; }
}

void TouchBarProcessor::ApplySyntheticKeys(HWKeyboard& kb)
{
    if (sk_.holdLeftAlt)   kb.Press(HWKeyboard::LEFT_ALT);
    if (sk_.holdLeftShift) kb.Press(HWKeyboard::LEFT_SHIFT);

    if (sk_.leftShiftFrames > 0) { kb.Press(HWKeyboard::LEFT_SHIFT); sk_.leftShiftFrames--; }
    if (sk_.leftCtrlFrames > 0)  { kb.Press(HWKeyboard::LEFT_CTRL);  sk_.leftCtrlFrames--; }
    if (sk_.leftGuiFrames > 0)   { kb.Press(HWKeyboard::LEFT_GUI);   sk_.leftGuiFrames--; }

    if (sk_.tabDelayFrames > 0)       sk_.tabDelayFrames--;
    else if (sk_.tabFrames > 0)       { kb.Press(HWKeyboard::TAB); sk_.tabFrames--; }

    if (sk_.leftArrowDelayFrames > 0)      sk_.leftArrowDelayFrames--;
    else if (sk_.leftArrowFrames > 0)      { kb.Press(HWKeyboard::LEFT_ARROW); sk_.leftArrowFrames--; }

    if (sk_.rightArrowDelayFrames > 0)     sk_.rightArrowDelayFrames--;
    else if (sk_.rightArrowFrames > 0)     { kb.Press(HWKeyboard::RIGHT_ARROW); sk_.rightArrowFrames--; }
}

void TouchBarProcessor::Process(uint32_t nowMs, HWKeyboard& kb)
{
    uint8_t touchState = kb.GetTouchBarState();
    uint8_t activeSegment = INVALID_SEGMENT;
    uint8_t activeTouchCount = 0;
    int16_t touchPosition = -1;

    if (s_.isTouching) {
        activeSegment = s_.activeSegment;
        if (activeSegment < SEGMENT_COUNT) {
            touchPosition = GetSegmentPosition(touchState, activeSegment);
            activeTouchCount = CountSegmentTouches(touchState, activeSegment);
        }
    } else {
        activeSegment = SelectSegment(touchState);
        if (activeSegment < SEGMENT_COUNT) {
            touchPosition = GetSegmentPosition(touchState, activeSegment);
            activeTouchCount = CountSegmentTouches(touchState, activeSegment);
        }
    }

    if (touchPosition < 0) {
        if (!s_.isTouching) goto apply;
        if (!s_.isNoTouchPending) {
            s_.isNoTouchPending = true;
            s_.lastTouchMs = nowMs;
            goto apply;
        }
        if (nowMs - s_.lastTouchMs < GetReleaseGraceMs()) goto apply;
        if (mode_ == TOUCHBAR_MODE_DESKTOP_SWITCH) FinalizeDesktopSwitchGesture();
        ClearActions();
        goto apply;
    }

    s_.isNoTouchPending = false;
    s_.lastTouchMs = nowMs;
    s_.currentPosition = touchPosition;

    if (mode_ == TOUCHBAR_MODE_APP_SWITCH) {
        bool isJitter = s_.isGestureActive && s_.activeTouchCount > activeTouchCount && activeTouchCount == 1;
        if (isJitter) s_.appSwitchReleaseGuardUntilMs = nowMs + APP_RELEASE_SETTLE_MS;
        else if (activeTouchCount > 1) s_.appSwitchReleaseGuardUntilMs = 0;
    }

    if (!s_.isTouching) {
        s_.isTouching = true;
        s_.activeSegment = activeSegment;
        s_.activeTouchCount = activeTouchCount;
        s_.touchStartMs = nowMs;
        s_.lastTouchMs = nowMs;
        s_.anchorPosition = touchPosition;
        s_.currentPosition = touchPosition;
        s_.emittedSteps = 0;
        s_.isDesktopSeekMode = false;
        s_.lastPanMs = nowMs;
        s_.lastStepMs = nowMs;
        s_.edgeHoldStartMs = 0;
        s_.edgeHoldDirection = 0;
        s_.appSwitchReleaseGuardUntilMs = 0;
        sk_.holdLeftAlt = false;
        sk_.holdLeftShift = false;
        sk_.hasShiftScrollPrimed = false;
        goto apply;
    }

    s_.activeTouchCount = activeTouchCount;

    if (!s_.isGestureActive) {
        if (nowMs - s_.touchStartMs < GetActivationDelayMs()) goto apply;
        s_.isGestureActive = true;
    }

    switch (mode_) {
        case TOUCHBAR_MODE_PAN:            HandlePanMode(nowMs, kb); break;
        case TOUCHBAR_MODE_APP_SWITCH:     HandleAppSwitchMode(nowMs); break;
        case TOUCHBAR_MODE_DESKTOP_SWITCH: HandleDesktopSwitchMode(nowMs); break;
        default: break;
    }

apply:
    ApplySyntheticKeys(kb);
}

bool TouchBar_HasPendingMouseReport()
{
    return touchBar.hasPendingMouse_;
}

bool TouchBar_TrySendMouseReport()
{
    if (!touchBar.hasPendingMouse_) return false;
    if (USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, touchBar.pendingMouseReport_,
                                    HWKeyboard::MOUSE_REPORT_SIZE) == USBD_OK) {
        touchBar.hasPendingMouse_ = false;
        return true;
    }
    return false;
}
