#ifndef PROFILE_STORE_H
#define PROFILE_STORE_H

#include <stdint.h>

static constexpr uint8_t PROFILE_MAX_SLOTS = 16;
static constexpr uint8_t PROFILE_NAME_LEN = 16;
static constexpr uint16_t PROFILE_DATA_SIZE = 256;

struct ProfileSlot_t {
    bool used = false;
    char name[PROFILE_NAME_LEN]{};
    uint32_t timestamp = 0;
    uint8_t kbParams[128]{};
    uint8_t kbParamLen = 0;
    uint8_t hubParams[128]{};
    uint8_t hubParamLen = 0;
};

class ProfileStore {
public:
    void Init();

    uint8_t GetSlotCount() const { return PROFILE_MAX_SLOTS; }
    bool IsSlotUsed(uint8_t slot) const;
    const char* GetSlotName(uint8_t slot) const;

    bool SaveProfile(uint8_t slot, const char* name);
    bool LoadProfile(uint8_t slot);
    bool DeleteProfile(uint8_t slot);

    // Serialize slot info for PC
    uint8_t GetProfileList(uint8_t* outBuf, uint16_t bufSize) const;

private:
    ProfileSlot_t slots_[PROFILE_MAX_SLOTS]{};
};

extern ProfileStore profileStore;

#endif // PROFILE_STORE_H
