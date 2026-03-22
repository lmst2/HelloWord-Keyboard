#ifndef CONFIG_CACHE_H
#define CONFIG_CACHE_H

#include <stdint.h>

// Cached copy of keyboard config on Hub side, updated via UART responses
class ConfigCache {
public:
    void Init();

    void UpdateParam(uint16_t paramId, const uint8_t* value, uint8_t len);
    void UpdateFromStatus(const uint8_t* statusPayload, uint8_t len);

    uint8_t GetParam(uint16_t paramId, uint8_t* outVal) const;

    // Quick accessors for commonly needed values
    uint8_t GetEffectMode() const { return effectMode_; }
    uint8_t GetBrightness() const { return brightness_; }
    uint8_t GetActiveLayer() const { return activeLayer_; }
    uint8_t GetOsMode() const { return osMode_; }
    uint8_t GetTouchBarMode() const { return touchBarMode_; }
    bool IsValid() const { return valid_; }

private:
    uint8_t effectMode_ = 0;
    uint8_t brightness_ = 4;
    uint8_t effectSpeed_ = 128;
    uint8_t activeLayer_ = 1;
    uint8_t osMode_ = 0xFF;
    uint8_t touchBarMode_ = 0;
    uint8_t fwVersion_ = 0;
    bool valid_ = false;
};

extern ConfigCache kbConfigCache;

#endif // CONFIG_CACHE_H
