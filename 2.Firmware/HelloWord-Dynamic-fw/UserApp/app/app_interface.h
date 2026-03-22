#ifndef APP_INTERFACE_H
#define APP_INTERFACE_H

#include <stdint.h>

// Forward declarations
class EinkCanvas;

enum KnobMotorMode : uint8_t {
    KNOB_DISABLE = 0,
    KNOB_INERTIA,
    KNOB_ENCODER,
    KNOB_SPRING,
    KNOB_DAMPED,
    KNOB_SPIN
};

enum EinkRefreshMode : uint8_t {
    EINK_FULL = 0,
    EINK_PARTIAL,
};

static constexpr uint8_t OS_BOTH    = 0xFF;
static constexpr uint8_t OS_WINDOWS = 0x00;
static constexpr uint8_t OS_MAC     = 0x01;

class IApp {
public:
    virtual ~IApp() = default;

    // ---- Identity ----
    virtual uint8_t     GetId() const = 0;
    virtual const char* GetName() const = 0;
    virtual const uint8_t* GetIcon16() const = 0;
    virtual uint8_t     GetOsSupport() const { return OS_BOTH; }

    // ---- Feature flags ----
    virtual uint8_t GetFeatures() const = 0;
    static constexpr uint8_t FEAT_KNOB  = 0x01;
    static constexpr uint8_t FEAT_MOTOR = 0x02;
    static constexpr uint8_t FEAT_EINK  = 0x04;
    static constexpr uint8_t FEAT_PC    = 0x08;

    // ---- Lifecycle ----
    virtual void OnActivate() {}
    virtual void OnDeactivate() {}
    virtual void OnEinkActivate() {}
    virtual void OnEinkDeactivate() {}

    // ---- Knob input (FEAT_KNOB) ----
    virtual void OnKnobDelta(int32_t delta) {}
    virtual KnobMotorMode GetMotorMode() const { return KNOB_ENCODER; }
    virtual int  GetPpr() const { return 24; }
    virtual float GetTorqueLimit() const { return 0.5f; }

    // ---- Motor output (FEAT_MOTOR) ----
    virtual void OnMotorTick(uint32_t nowMs) {}

    // ---- E-Ink rendering (FEAT_EINK) ----
    virtual void OnEinkRender(EinkCanvas& canvas) {}
    virtual bool NeedsEinkRefresh() const { return false; }
    virtual EinkRefreshMode GetRefreshMode() const { return EINK_PARTIAL; }

    // ---- PC data feed (FEAT_PC) ----
    virtual void OnPcData(uint8_t feedId, const uint8_t* data, uint8_t len) {}

    // ---- General tick ----
    virtual void OnTick(uint32_t nowMs) {}

    // ---- L2 Submenu ----
    virtual uint8_t GetSubItemCount() const { return 0; }
    virtual const char* GetSubItemName(uint8_t idx) const { return nullptr; }
    virtual const uint8_t* GetSubItemIcon(uint8_t idx) const { return nullptr; }
    virtual uint8_t GetActiveSubItem() const { return 0; }
    virtual void OnSubItemSelected(uint8_t idx) {}
};

#endif // APP_INTERFACE_H
