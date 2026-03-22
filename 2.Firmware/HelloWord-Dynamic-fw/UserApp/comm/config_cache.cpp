#include "config_cache.h"
#include "config_params.h"

ConfigCache kbConfigCache;

void ConfigCache::Init()
{
    valid_ = false;
}

void ConfigCache::UpdateParam(uint16_t paramId, const uint8_t* value, uint8_t len)
{
    if (len < 1) return;

    switch (paramId) {
        case PARAM_EFFECT_MODE:    effectMode_ = value[0]; break;
        case PARAM_BRIGHTNESS:     brightness_ = value[0]; break;
        case PARAM_EFFECT_SPEED:   effectSpeed_ = value[0]; break;
        case PARAM_ACTIVE_LAYER:   activeLayer_ = value[0]; break;
        case PARAM_OS_MODE:        osMode_ = value[0]; break;
        case PARAM_TOUCHBAR_MODE:  touchBarMode_ = value[0]; break;
        default: break;
    }
    valid_ = true;
}

void ConfigCache::UpdateFromStatus(const uint8_t* statusPayload, uint8_t len)
{
    if (len < 6) return;
    fwVersion_ = statusPayload[0];
    effectMode_ = statusPayload[1];
    brightness_ = statusPayload[2];
    activeLayer_ = statusPayload[3];
    osMode_ = statusPayload[4];
    touchBarMode_ = statusPayload[5];
    valid_ = true;
}

uint8_t ConfigCache::GetParam(uint16_t paramId, uint8_t* outVal) const
{
    switch (paramId) {
        case PARAM_EFFECT_MODE:    outVal[0] = effectMode_; return 1;
        case PARAM_BRIGHTNESS:     outVal[0] = brightness_; return 1;
        case PARAM_EFFECT_SPEED:   outVal[0] = effectSpeed_; return 1;
        case PARAM_ACTIVE_LAYER:   outVal[0] = activeLayer_; return 1;
        case PARAM_OS_MODE:        outVal[0] = osMode_; return 1;
        case PARAM_TOUCHBAR_MODE:  outVal[0] = touchBarMode_; return 1;
        default: return 0;
    }
}
