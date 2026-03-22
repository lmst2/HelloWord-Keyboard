#include "kb_device_log.h"
#include "uart_comm.h"
#include "protocol.h"
#include <cstring>

static bool g_enabled = false;
static uint8_t g_maxLevel = 2;

void KbDeviceLogApplyFromHub(uint8_t enabled, uint8_t maxLevel)
{
    g_enabled = (enabled != 0);
    g_maxLevel = maxLevel > 3 ? 3 : maxLevel;
}

bool KbDeviceLogShouldEmit(uint8_t level)
{
    if (!g_enabled)
        return false;
    if (level > 3)
        level = 3;
    return level <= g_maxLevel;
}

void KbDeviceLogLine(uint8_t level, const char* text)
{
    if (!KbDeviceLogShouldEmit(level) || !text)
        return;

    uint8_t payload[120];
    payload[0] = level;
    size_t n = strlen(text);
    if (n > sizeof(payload) - 1)
        n = sizeof(payload) - 1;
    memcpy(payload + 1, text, n);
    uartComm.Send(Msg::KB_HUB_LOG, payload, (uint8_t)(1 + n));
}
