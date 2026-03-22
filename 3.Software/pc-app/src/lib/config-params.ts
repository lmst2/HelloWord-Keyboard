// Mirrors firmware config_params.h — param IDs

export const ConfigParam = {
  // Lighting (0x0100)
  PARAM_EFFECT_MODE: 0x0100,
  PARAM_BRIGHTNESS: 0x0101,
  PARAM_EFFECT_SPEED: 0x0106,
  PARAM_EFX_RAINBOW_HUE_OFS: 0x0110,
  PARAM_EFX_REACTIVE_H: 0x0120,
  PARAM_EFX_REACTIVE_S: 0x0121,
  PARAM_EFX_AURORA_TINT_H: 0x0130,
  PARAM_EFX_RIPPLE_H: 0x0140,
  PARAM_EFX_STATIC_R: 0x0150,
  PARAM_EFX_STATIC_G: 0x0151,
  PARAM_EFX_STATIC_B: 0x0152,

  // TouchBar (0x0200)
  PARAM_TOUCHBAR_MODE: 0x0200,
  PARAM_TB_ACTIVATION_MS: 0x0201,
  PARAM_TB_RELEASE_GRACE: 0x0202,

  // Keymap (0x0300)
  PARAM_ACTIVE_LAYER: 0x0300,
  PARAM_OS_MODE: 0x0301,

  // Sleep (0x0400)
  PARAM_SLEEP_TIMEOUT_MIN: 0x0400,
  PARAM_SLEEP_FADE_MS: 0x0401,
  PARAM_SLEEP_BREATHE_MS: 0x0402,

  // Hub (0x0500)
  PARAM_HUB_OLED_BRIGHTNESS: 0x0500,
  PARAM_HUB_STANDBY_TIMEOUT: 0x0501,
  PARAM_HUB_PRIMARY_APP_ID: 0x0520,
  PARAM_HUB_EINK_APP_ID: 0x0521,
} as const;

export const EFFECT_NAMES = [
  "Rainbow Sweep",
  "Reactive",
  "Aurora",
  "Ripple",
  "Static",
  "Breathing",
  "Wave",
  "Off",
] as const;

export const TOUCHBAR_MODES = [
  "Off",
  "Mouse Wheel",
  "Volume",
  "Brightness",
] as const;

export const OS_MODES = ["Windows", "macOS", "Auto"] as const;
