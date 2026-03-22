#ifndef DEVICE_LOG_H
#define DEVICE_LOG_H

#include <stdint.h>

// Levels: 0=error, 1=warn, 2=info, 3=debug (filter: emit if level <= configured max)
void DeviceLogApplyFromPc(uint8_t enabled, uint8_t maxLevel);
bool DeviceLogShouldEmit(uint8_t level);
void DeviceLogEmitHub(uint8_t level, const char* text);
void DeviceLogEmitFromKeyboard(uint8_t level, const uint8_t* text, uint8_t textLen);

#endif // DEVICE_LOG_H
