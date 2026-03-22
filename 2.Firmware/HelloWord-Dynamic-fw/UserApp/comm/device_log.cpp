#include "device_log.h"
#include "protocol.h"
#include "hub_usb_comm.h"
#include "hub_uart_comm.h"
#include <cstring>
#include <cstdio>

static bool g_enabled = false;
static uint8_t g_maxLevel = 2; // default info cap when enabled

static void SendLogPayload(uint8_t source, uint8_t level, const char* text, size_t textLen)
{
    if (!text || textLen == 0)
        return;
    uint8_t resp[124];
    resp[0] = Msg::HUB_PC_LOG;
    resp[1] = source;
    resp[2] = level;
    if (textLen > sizeof(resp) - 3)
        textLen = sizeof(resp) - 3;
    memcpy(resp + 3, text, textLen);
    hubUsb.SendResponse(resp, (uint16_t)(3 + textLen));
}

void DeviceLogApplyFromPc(uint8_t enabled, uint8_t maxLevel)
{
    g_enabled = (enabled != 0);
    if (maxLevel > 3)
        maxLevel = 3;
    g_maxLevel = maxLevel;

    uint8_t p[2] = {g_enabled ? (uint8_t)1 : (uint8_t)0, g_maxLevel};
    hubUart.Send(Msg::HUB_KB_LOG_CONFIG, p, 2);

    char ack[56];
    snprintf(ack, sizeof(ack), "log cfg en=%u max=%u", (unsigned)g_enabled, (unsigned)g_maxLevel);
    SendLogPayload(0, 2, ack, strlen(ack));
}

bool DeviceLogShouldEmit(uint8_t level)
{
    if (!g_enabled)
        return false;
    if (level > 3)
        level = 3;
    return level <= g_maxLevel;
}

void DeviceLogEmitHub(uint8_t level, const char* text)
{
    if (!DeviceLogShouldEmit(level) || !text)
        return;
    size_t n = strlen(text);
    SendLogPayload(0, level, text, n);
}

void DeviceLogEmitFromKeyboard(uint8_t level, const uint8_t* text, uint8_t textLen)
{
    if (!DeviceLogShouldEmit(level) || !text || textLen == 0)
        return;
    uint8_t resp[124];
    resp[0] = Msg::HUB_PC_LOG;
    resp[1] = 1;
    resp[2] = level;
    if (textLen > sizeof(resp) - 3)
        textLen = (uint8_t)(sizeof(resp) - 3);
    memcpy(resp + 3, text, textLen);
    hubUsb.SendResponse(resp, (uint16_t)(3 + textLen));
}
