#include "profile_store.h"
#include "hub_config.h"
#include "comm/hub_uart_comm.h"
#include "comm/config_params.h"
#include <string.h>

ProfileStore profileStore;

void ProfileStore::Init()
{
    // TODO: load profile index from Flash
    for (uint8_t i = 0; i < PROFILE_MAX_SLOTS; i++)
        slots_[i].used = false;
}

bool ProfileStore::IsSlotUsed(uint8_t slot) const
{
    return slot < PROFILE_MAX_SLOTS && slots_[slot].used;
}

const char* ProfileStore::GetSlotName(uint8_t slot) const
{
    if (slot >= PROFILE_MAX_SLOTS || !slots_[slot].used) return nullptr;
    return slots_[slot].name;
}

bool ProfileStore::SaveProfile(uint8_t slot, const char* name)
{
    if (slot >= PROFILE_MAX_SLOTS) return false;

    ProfileSlot_t& s = slots_[slot];
    s.used = true;
    strncpy(s.name, name, PROFILE_NAME_LEN - 1);
    s.name[PROFILE_NAME_LEN - 1] = '\0';

    // Snapshot hub config
    s.hubParamLen = 0;
    auto& data = hubConfig.Data();
    s.hubParams[s.hubParamLen++] = data.primaryAppId;
    s.hubParams[s.hubParamLen++] = data.einkAppId;
    s.hubParams[s.hubParamLen++] = data.oledBrightness;
    s.hubParams[s.hubParamLen++] = data.torqueLimit;

    // Request keyboard GET_ALL for snapshot (async — cache is used)
    hubUart.SendConfigGetAll();

    // Use cached keyboard params
    auto& cache = hubConfig.GetKbCache();
    s.kbParamLen = 0;
    uint8_t val[4];
    static const uint16_t kbParams[] = {
        PARAM_EFFECT_MODE, PARAM_BRIGHTNESS, PARAM_EFFECT_SPEED,
        PARAM_TOUCHBAR_MODE, PARAM_ACTIVE_LAYER, PARAM_OS_MODE,
        PARAM_SENTINEL
    };
    for (int i = 0; kbParams[i] != PARAM_SENTINEL; i++) {
        uint8_t vlen = cache.GetParam(kbParams[i], val);
        if (vlen > 0 && s.kbParamLen + 3 + vlen < 128) {
            s.kbParams[s.kbParamLen++] = (uint8_t)(kbParams[i] >> 8);
            s.kbParams[s.kbParamLen++] = (uint8_t)(kbParams[i] & 0xFF);
            s.kbParams[s.kbParamLen++] = vlen;
            for (uint8_t j = 0; j < vlen; j++) s.kbParams[s.kbParamLen++] = val[j];
        }
    }

    // TODO: persist to Flash
    return true;
}

bool ProfileStore::LoadProfile(uint8_t slot)
{
    if (slot >= PROFILE_MAX_SLOTS || !slots_[slot].used) return false;

    ProfileSlot_t& s = slots_[slot];

    // Restore hub config
    if (s.hubParamLen >= 4) {
        hubConfig.Data().primaryAppId = s.hubParams[0];
        hubConfig.Data().einkAppId = s.hubParams[1];
        hubConfig.Data().oledBrightness = s.hubParams[2];
        hubConfig.Data().torqueLimit = s.hubParams[3];
    }

    // Push keyboard params via UART
    uint8_t pos = 0;
    while (pos + 3 <= s.kbParamLen) {
        uint16_t paramId = ((uint16_t)s.kbParams[pos] << 8) | s.kbParams[pos + 1];
        uint8_t vlen = s.kbParams[pos + 2];
        pos += 3;
        if (pos + vlen > s.kbParamLen) break;
        hubUart.SendConfigSet(paramId, s.kbParams + pos, vlen);
        pos += vlen;
    }

    return true;
}

bool ProfileStore::DeleteProfile(uint8_t slot)
{
    if (slot >= PROFILE_MAX_SLOTS) return false;
    slots_[slot].used = false;
    memset(slots_[slot].name, 0, PROFILE_NAME_LEN);
    // TODO: erase from Flash
    return true;
}

uint8_t ProfileStore::GetProfileList(uint8_t* outBuf, uint16_t bufSize) const
{
    uint8_t pos = 0;
    outBuf[pos++] = PROFILE_MAX_SLOTS;
    for (uint8_t i = 0; i < PROFILE_MAX_SLOTS && pos + 18 < bufSize; i++) {
        outBuf[pos++] = i;
        outBuf[pos++] = slots_[i].used ? 1 : 0;
        memcpy(outBuf + pos, slots_[i].name, PROFILE_NAME_LEN);
        pos += PROFILE_NAME_LEN;
    }
    return pos;
}
