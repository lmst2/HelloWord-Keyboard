#ifndef CONFIG_PARAMS_H
#define CONFIG_PARAMS_H

#include <stdint.h>

enum ConfigParam : uint16_t {
    // ---- Keyboard params (0x0100-0x04FF) ---- stored in KB EEPROM
    // Lighting (0x0100)
    PARAM_EFFECT_MODE         = 0x0100,
    PARAM_BRIGHTNESS          = 0x0101,
    PARAM_EFFECT_SPEED        = 0x0106,
    // Per-effect color (0x0110-0x015F)
    PARAM_EFX_RAINBOW_HUE_OFS= 0x0110,
    PARAM_EFX_REACTIVE_H      = 0x0120,
    PARAM_EFX_REACTIVE_S      = 0x0121,
    PARAM_EFX_AURORA_TINT_H   = 0x0130,
    PARAM_EFX_RIPPLE_H        = 0x0140,
    PARAM_EFX_STATIC_R        = 0x0150,
    PARAM_EFX_STATIC_G        = 0x0151,
    PARAM_EFX_STATIC_B        = 0x0152,
    // TouchBar (0x0200)
    PARAM_TOUCHBAR_MODE       = 0x0200,
    PARAM_TB_ACTIVATION_MS    = 0x0201,
    PARAM_TB_RELEASE_GRACE    = 0x0202,
    // Keymap (0x0300)
    PARAM_ACTIVE_LAYER        = 0x0300,
    PARAM_OS_MODE             = 0x0301,
    PARAM_FN_BEHAVIOR         = 0x0302,
    // Sleep (0x0400)
    PARAM_SLEEP_TIMEOUT_MIN   = 0x0400,
    PARAM_SLEEP_FADE_MS       = 0x0401,
    PARAM_SLEEP_BREATHE_MS    = 0x0402,

    // ---- Hub params (0x0500-0x08FF) ---- stored in Hub Flash
    PARAM_HUB_OLED_BRIGHTNESS = 0x0500,
    PARAM_HUB_STANDBY_TIMEOUT = 0x0501,
    PARAM_HUB_UART_ENABLED    = 0x0502,
    PARAM_HUB_HEARTBEAT_SEC   = 0x0503,
    PARAM_HUB_TORQUE_LIMIT    = 0x0510,
    PARAM_HUB_DEFAULT_PPR     = 0x0511,
    PARAM_HUB_PRIMARY_APP_ID  = 0x0520,
    PARAM_HUB_EINK_APP_ID     = 0x0521,
    // Per-app motor mode (0x0600 + app_id)
    PARAM_APP_MOTOR_MODE_BASE = 0x0600,
    // Per-eink-app sub-selection (0x0700 + app_id)
    PARAM_APP_EINK_SUB_BASE   = 0x0700,

    PARAM_SENTINEL            = 0xFFFF
};

// Keyboard param range check
static inline bool IsKeyboardParam(uint16_t paramId) {
    return paramId >= 0x0100 && paramId <= 0x04FF;
}

static inline bool IsHubParam(uint16_t paramId) {
    return paramId >= 0x0500 && paramId <= 0x08FF;
}

#endif // CONFIG_PARAMS_H
