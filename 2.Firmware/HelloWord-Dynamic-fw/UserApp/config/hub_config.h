#ifndef HUB_CONFIG_H
#define HUB_CONFIG_H

#include <stdint.h>
#include "comm/config_params.h"
#include "comm/config_cache.h"

struct HubConfigData_t
{
    uint8_t oledBrightness = 255;
    uint16_t standbyTimeoutMs = 60000;
    uint8_t uartEnabled = 1;
    uint8_t heartbeatSec = 5;
    uint8_t torqueLimit = 50;   // x0.01 -> 0.50
    uint8_t defaultPpr = 24;
    uint8_t primaryAppId = 0;
    uint8_t einkAppId = 0;
    uint8_t appMotorModes[24]{};
    uint8_t appEinkSubs[24]{};
};

class HubConfig {
public:
    void Init();

    uint8_t GetParam(uint16_t paramId, uint8_t* outVal) const;
    bool SetParam(uint16_t paramId, const uint8_t* value, uint8_t len);

    void Save();
    void LoadDefaults();

    HubConfigData_t& Data() { return data_; }
    const HubConfigData_t& Data() const { return data_; }

    ConfigCache& GetKbCache() { return kbCache_; }

private:
    HubConfigData_t data_{};
    ConfigCache kbCache_{};
    bool dirty_ = false;
};

extern HubConfig hubConfig;

#endif // HUB_CONFIG_H
