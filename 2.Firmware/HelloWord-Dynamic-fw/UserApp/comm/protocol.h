#ifndef PROTOCOL_H
#define PROTOCOL_H

// Shared protocol header — identical to keyboard-fw version
#include <stdint.h>

namespace Msg {

// ==== PC <-> Keyboard (USB HID Raw) ====
static constexpr uint8_t PC_KB_RGB_MODE       = 0x01;
static constexpr uint8_t PC_KB_RGB_DIRECT     = 0x02;
static constexpr uint8_t PC_KB_CONFIG_GET     = 0x05;
static constexpr uint8_t PC_KB_CONFIG_SET     = 0x06;
static constexpr uint8_t PC_KB_STATUS_REQ     = 0x07;
static constexpr uint8_t PC_KB_CONFIG_GET_ALL = 0x08;
static constexpr uint8_t PC_KB_DFU            = 0xDF;
static constexpr uint8_t KB_PC_STATUS         = 0x81;
static constexpr uint8_t KB_PC_CONFIG_VALUE   = 0x82;
static constexpr uint8_t KB_PC_ACK            = 0x83;
static constexpr uint8_t LEGACY_RGB_DIRECT    = 0xAC;
static constexpr uint8_t LEGACY_RGB_STOP      = 0xBD;

// ==== Hub <-> Keyboard (UART SLIP) ====
static constexpr uint8_t HUB_KB_RGB_MODE      = 0x21;
static constexpr uint8_t HUB_KB_RGB_DIRECT    = 0x22;
static constexpr uint8_t HUB_KB_STATUS_REQ    = 0x24;
static constexpr uint8_t HUB_KB_CONFIG_GET    = 0x25;
static constexpr uint8_t HUB_KB_CONFIG_SET    = 0x26;
static constexpr uint8_t HUB_KB_CONFIG_GET_ALL= 0x28;
static constexpr uint8_t HUB_KB_KEY_ACTION    = 0x29;
static constexpr uint8_t HUB_KB_DFU           = 0x2F;
static constexpr uint8_t KB_HUB_FN_STATE      = 0xA1;
static constexpr uint8_t KB_HUB_STATUS        = 0xA3;
static constexpr uint8_t KB_HUB_KEY_EVENT     = 0xA4;
static constexpr uint8_t KB_HUB_TOUCHBAR      = 0xA5;
static constexpr uint8_t KB_HUB_CONFIG_VALUE  = 0xA6;
static constexpr uint8_t KB_HUB_CONFIG_ACK    = 0xA7;
static constexpr uint8_t KB_HUB_PING          = 0xAF;

// ==== PC <-> Hub (USB CDC) ====
static constexpr uint8_t PC_HUB_CONFIG_GET    = 0xC1;
static constexpr uint8_t PC_HUB_CONFIG_SET    = 0xC2;
static constexpr uint8_t PC_HUB_CONFIG_GET_ALL= 0xC3;
static constexpr uint8_t PC_HUB_STATUS_REQ    = 0xC4;
static constexpr uint8_t PC_HUB_DATA_FEED     = 0xC5;
static constexpr uint8_t PC_HUB_EINK_IMAGE    = 0xC6;
static constexpr uint8_t PC_HUB_EINK_TEXT     = 0xC7;
static constexpr uint8_t PC_HUB_FW_INFO_REQ   = 0xC8;
static constexpr uint8_t PC_HUB_DFU_KB        = 0xC9;
static constexpr uint8_t PC_HUB_DFU_HUB       = 0xCA;
static constexpr uint8_t PC_HUB_PROFILE_LIST  = 0xCB;
static constexpr uint8_t PC_HUB_PROFILE_SAVE  = 0xCC;
static constexpr uint8_t PC_HUB_PROFILE_LOAD  = 0xCD;
static constexpr uint8_t PC_HUB_PROFILE_DELETE= 0xCE;
static constexpr uint8_t PC_HUB_APP_SWITCH    = 0xD1;
static constexpr uint8_t PC_HUB_EINK_SWITCH   = 0xD2;
static constexpr uint8_t PC_HUB_RGB_FORWARD   = 0xD3;
static constexpr uint8_t HUB_PC_CONFIG_VALUE  = 0xE1;
static constexpr uint8_t HUB_PC_STATUS        = 0xE2;
static constexpr uint8_t HUB_PC_ACK           = 0xE3;
static constexpr uint8_t HUB_PC_FW_INFO       = 0xE4;
static constexpr uint8_t HUB_PC_PROFILE_LIST  = 0xE5;
static constexpr uint8_t HUB_PC_STATE_EVENT   = 0xE6;

static constexpr uint8_t RESULT_OK            = 0x00;
static constexpr uint8_t RESULT_ERR_PARAM     = 0x01;
static constexpr uint8_t RESULT_ERR_RANGE     = 0x02;
static constexpr uint8_t RESULT_ERR_BUSY      = 0x03;

static constexpr uint8_t TARGET_KEYBOARD      = 0x00;
static constexpr uint8_t TARGET_HUB           = 0x01;

static constexpr uint8_t KEY_ACTION_PRESS     = 0x01;
static constexpr uint8_t KEY_ACTION_RELEASE   = 0x02;
static constexpr uint8_t KEY_ACTION_TAP       = 0x03;

static constexpr uint8_t OS_WINDOWS           = 0x00;
static constexpr uint8_t OS_MAC               = 0x01;
static constexpr uint8_t OS_BOTH              = 0xFF;

} // namespace Msg

#endif // PROTOCOL_H
