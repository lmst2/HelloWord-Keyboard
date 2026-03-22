#include "config_handler.h"
#include "common_inc.h"
#include "HelloWord/hw_keyboard.h"

extern HWKeyboard keyboard;
static EEPROM configEeprom;

void ConfigHandler::Init(KeyboardConfig_t* cfg)
{
    cfg_ = cfg;
    dirty_ = false;
}

uint8_t ConfigHandler::GetParam(uint16_t paramId, uint8_t* outVal)
{
    if (!cfg_) return 0;

    switch (paramId) {
        case PARAM_EFFECT_MODE:    outVal[0] = cfg_->effectMode; return 1;
        case PARAM_BRIGHTNESS:     outVal[0] = cfg_->brightness; return 1;
        case PARAM_EFFECT_SPEED:   outVal[0] = cfg_->effectSpeed; return 1;

        case PARAM_EFX_RAINBOW_HUE_OFS: outVal[0] = cfg_->effectColors.rainbowHueOffset; return 1;
        case PARAM_EFX_REACTIVE_H: outVal[0] = cfg_->effectColors.reactiveH; return 1;
        case PARAM_EFX_REACTIVE_S: outVal[0] = cfg_->effectColors.reactiveS; return 1;
        case PARAM_EFX_AURORA_TINT_H: outVal[0] = cfg_->effectColors.auroraTintH; return 1;
        case PARAM_EFX_RIPPLE_H:   outVal[0] = cfg_->effectColors.rippleH; return 1;
        case PARAM_EFX_STATIC_R:   outVal[0] = cfg_->effectColors.staticR; return 1;
        case PARAM_EFX_STATIC_G:   outVal[0] = cfg_->effectColors.staticG; return 1;
        case PARAM_EFX_STATIC_B:   outVal[0] = cfg_->effectColors.staticB; return 1;

        case PARAM_TOUCHBAR_MODE:  outVal[0] = cfg_->touchBar.mode; return 1;
        case PARAM_TB_ACTIVATION_MS:
            outVal[0] = (uint8_t)(cfg_->touchBar.activationMs & 0xFF);
            outVal[1] = (uint8_t)(cfg_->touchBar.activationMs >> 8);
            return 2;
        case PARAM_TB_RELEASE_GRACE:
            outVal[0] = (uint8_t)(cfg_->touchBar.releaseGraceMs & 0xFF);
            outVal[1] = (uint8_t)(cfg_->touchBar.releaseGraceMs >> 8);
            return 2;

        case PARAM_ACTIVE_LAYER:   outVal[0] = cfg_->activeLayer; return 1;
        case PARAM_OS_MODE:        outVal[0] = cfg_->osMode; return 1;

        case PARAM_SLEEP_TIMEOUT_MIN: outVal[0] = cfg_->sleep.timeoutMin; return 1;
        case PARAM_SLEEP_FADE_MS:
            outVal[0] = (uint8_t)(cfg_->sleep.fadeMs & 0xFF);
            outVal[1] = (uint8_t)(cfg_->sleep.fadeMs >> 8);
            return 2;
        case PARAM_SLEEP_BREATHE_MS:
            outVal[0] = (uint8_t)(cfg_->sleep.breatheMs & 0xFF);
            outVal[1] = (uint8_t)(cfg_->sleep.breatheMs >> 8);
            return 2;

        default: return 0;
    }
}

bool ConfigHandler::SetParam(uint16_t paramId, const uint8_t* value, uint8_t len)
{
    if (!cfg_ || len == 0) return false;

    switch (paramId) {
        case PARAM_EFFECT_MODE:
            if (value[0] >= 5) return false;
            cfg_->effectMode = value[0];
            keyboard.SetEffect((HWKeyboard::LightEffect_t)value[0]);
            break;
        case PARAM_BRIGHTNESS:
            cfg_->brightness = value[0];
            keyboard.SetBrightnessLevel(value[0]);
            break;
        case PARAM_EFFECT_SPEED:
            cfg_->effectSpeed = value[0];
            break;

        case PARAM_EFX_RAINBOW_HUE_OFS: cfg_->effectColors.rainbowHueOffset = value[0]; break;
        case PARAM_EFX_REACTIVE_H: cfg_->effectColors.reactiveH = value[0]; break;
        case PARAM_EFX_REACTIVE_S: cfg_->effectColors.reactiveS = value[0]; break;
        case PARAM_EFX_AURORA_TINT_H: cfg_->effectColors.auroraTintH = value[0]; break;
        case PARAM_EFX_RIPPLE_H: cfg_->effectColors.rippleH = value[0]; break;
        case PARAM_EFX_STATIC_R: cfg_->effectColors.staticR = value[0]; break;
        case PARAM_EFX_STATIC_G: cfg_->effectColors.staticG = value[0]; break;
        case PARAM_EFX_STATIC_B: cfg_->effectColors.staticB = value[0]; break;

        case PARAM_TOUCHBAR_MODE:
            if (value[0] >= 3) return false;
            cfg_->touchBar.mode = value[0];
            break;
        case PARAM_TB_ACTIVATION_MS:
            if (len < 2) return false;
            cfg_->touchBar.activationMs = value[0] | ((uint16_t)value[1] << 8);
            break;
        case PARAM_TB_RELEASE_GRACE:
            if (len < 2) return false;
            cfg_->touchBar.releaseGraceMs = value[0] | ((uint16_t)value[1] << 8);
            break;

        case PARAM_ACTIVE_LAYER: cfg_->activeLayer = value[0]; break;
        case PARAM_OS_MODE: cfg_->osMode = value[0]; break;

        case PARAM_SLEEP_TIMEOUT_MIN: cfg_->sleep.timeoutMin = value[0]; break;
        case PARAM_SLEEP_FADE_MS:
            if (len < 2) return false;
            cfg_->sleep.fadeMs = value[0] | ((uint16_t)value[1] << 8);
            break;
        case PARAM_SLEEP_BREATHE_MS:
            if (len < 2) return false;
            cfg_->sleep.breatheMs = value[0] | ((uint16_t)value[1] << 8);
            break;

        default: return false;
    }

    dirty_ = true;
    return true;
}

uint16_t ConfigHandler::GetAllParams(uint8_t* outBuf, uint16_t bufSize)
{
    if (!cfg_ || bufSize < 4) return 0;

    static const uint16_t paramIds[] = {
        PARAM_EFFECT_MODE, PARAM_BRIGHTNESS, PARAM_EFFECT_SPEED,
        PARAM_EFX_RAINBOW_HUE_OFS, PARAM_EFX_REACTIVE_H, PARAM_EFX_REACTIVE_S,
        PARAM_EFX_AURORA_TINT_H, PARAM_EFX_RIPPLE_H,
        PARAM_EFX_STATIC_R, PARAM_EFX_STATIC_G, PARAM_EFX_STATIC_B,
        PARAM_TOUCHBAR_MODE, PARAM_TB_ACTIVATION_MS, PARAM_TB_RELEASE_GRACE,
        PARAM_ACTIVE_LAYER, PARAM_OS_MODE,
        PARAM_SLEEP_TIMEOUT_MIN, PARAM_SLEEP_FADE_MS, PARAM_SLEEP_BREATHE_MS,
        PARAM_SENTINEL
    };

    uint16_t pos = 0;
    for (int i = 0; paramIds[i] != PARAM_SENTINEL; i++) {
        uint8_t val[4];
        uint8_t vlen = GetParam(paramIds[i], val);
        if (vlen == 0) continue;

        // [paramId_hi, paramId_lo, len, value...]
        uint16_t needed = 3 + vlen;
        if (pos + needed > bufSize) break;

        outBuf[pos++] = (uint8_t)(paramIds[i] >> 8);
        outBuf[pos++] = (uint8_t)(paramIds[i] & 0xFF);
        outBuf[pos++] = vlen;
        for (uint8_t j = 0; j < vlen; j++)
            outBuf[pos++] = val[j];
    }
    return pos;
}

void ConfigHandler::ApplyToRuntime()
{
    if (!cfg_) return;
    keyboard.SetEffect((HWKeyboard::LightEffect_t)cfg_->effectMode);
    keyboard.SetBrightnessLevel(cfg_->brightness);
}

void ConfigHandler::Save()
{
    if (!cfg_ || !dirty_) return;
    cfg_->configStatus = CONFIG_OK;
    configEeprom.Push(0, *cfg_);
    dirty_ = false;
}
