#ifndef CONFIG_HANDLER_H
#define CONFIG_HANDLER_H

#include <stdint.h>
#include "configurations.h"
#include "config_params.h"

class ConfigHandler {
public:
    void Init(KeyboardConfig_t* cfg);

    // Get param value. Returns byte count written to outVal, or 0 if unknown.
    uint8_t GetParam(uint16_t paramId, uint8_t* outVal);

    // Set param value. Returns true if accepted and applied.
    bool SetParam(uint16_t paramId, const uint8_t* value, uint8_t len);

    // Serialize all keyboard params into buffer. Returns total bytes written.
    uint16_t GetAllParams(uint8_t* outBuf, uint16_t bufSize);

    // Apply runtime config from stored config struct
    void ApplyToRuntime();

    // Mark config dirty; call Save() to persist.
    void MarkDirty() { dirty_ = true; }
    bool IsDirty() const { return dirty_; }

    // Persist config to EEPROM
    void Save();

private:
    KeyboardConfig_t* cfg_ = nullptr;
    bool dirty_ = false;
};

#endif // CONFIG_HANDLER_H
