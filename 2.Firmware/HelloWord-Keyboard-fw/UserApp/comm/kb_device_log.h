#ifndef KB_DEVICE_LOG_H
#define KB_DEVICE_LOG_H

#include <stdint.h>

void KbDeviceLogApplyFromHub(uint8_t enabled, uint8_t maxLevel);
bool KbDeviceLogShouldEmit(uint8_t level);
void KbDeviceLogLine(uint8_t level, const char* text);

#endif // KB_DEVICE_LOG_H
