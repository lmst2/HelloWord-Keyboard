#ifndef CONFIGURATIONS_H
#define CONFIGURATIONS_H

#ifdef __cplusplus
extern "C" {
#endif
/*---------------------------- C Scope ---------------------------*/
#include <stdbool.h>
#include "stdint-gcc.h"


typedef enum configStatus_t
{
    CONFIG_RESTORE = 0,
    CONFIG_OK,
    CONFIG_COMMIT
} configStatus_t;


struct EffectColorConfig_t
{
    uint8_t rainbowHueOffset;
    uint8_t reactiveH;
    uint8_t reactiveS;
    uint8_t auroraTintH;
    uint8_t rippleH;
    uint8_t staticR;
    uint8_t staticG;
    uint8_t staticB;
};

struct TouchBarConfig_t
{
    uint8_t mode;
    uint16_t activationMs;
    uint16_t releaseGraceMs;
};

struct SleepConfig_t
{
    uint8_t timeoutMin;
    uint16_t fadeMs;
    uint16_t breatheMs;
};

typedef struct KeyboardConfig_t
{
    configStatus_t configStatus;
    uint64_t serialNum;
    int8_t keyMap[128];

    // Lighting
    uint8_t effectMode;
    uint8_t brightness;
    uint8_t effectSpeed;
    EffectColorConfig_t effectColors;

    // TouchBar
    TouchBarConfig_t touchBar;

    // Keymap
    uint8_t activeLayer;
    uint8_t osMode;

    // Sleep
    SleepConfig_t sleep;
} KeyboardConfig_t;

extern KeyboardConfig_t config;


static inline KeyboardConfig_t GetDefaultConfig()
{
    KeyboardConfig_t c{};
    c.configStatus = CONFIG_OK;
    c.serialNum = 123;
    for (int i = 0; i < 128; i++) c.keyMap[i] = -1;

    c.effectMode = 0;
    c.brightness = 4;
    c.effectSpeed = 128;
    c.effectColors = {0, 128, 200, 110, 0, 255, 180, 80};

    c.touchBar = {0, 20, 35};

    c.activeLayer = 1;
    c.osMode = 0xFF;

    c.sleep = {5, 800, 4800};

    return c;
}


#ifdef __cplusplus
}
/*---------------------------- C++ Scope ---------------------------*/


#endif
#endif
