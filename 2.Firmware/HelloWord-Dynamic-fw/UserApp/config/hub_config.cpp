#include "hub_config.h"

HubConfig hubConfig;

void HubConfig::Init()
{
    LoadDefaults();
    kbCache_.Init();
}

void HubConfig::LoadDefaults()
{
    data_ = HubConfigData_t{};
    dirty_ = false;
}

uint8_t HubConfig::GetParam(uint16_t paramId, uint8_t* outVal) const
{
    if (IsKeyboardParam(paramId))
        return kbCache_.GetParam(paramId, outVal);

    switch (paramId) {
        case PARAM_HUB_OLED_BRIGHTNESS: outVal[0] = data_.oledBrightness; return 1;
        case PARAM_HUB_STANDBY_TIMEOUT:
            outVal[0] = (uint8_t)(data_.standbyTimeoutMs & 0xFF);
            outVal[1] = (uint8_t)(data_.standbyTimeoutMs >> 8);
            return 2;
        case PARAM_HUB_UART_ENABLED: outVal[0] = data_.uartEnabled; return 1;
        case PARAM_HUB_HEARTBEAT_SEC: outVal[0] = data_.heartbeatSec; return 1;
        case PARAM_HUB_TORQUE_LIMIT: outVal[0] = data_.torqueLimit; return 1;
        case PARAM_HUB_DEFAULT_PPR: outVal[0] = data_.defaultPpr; return 1;
        case PARAM_HUB_PRIMARY_APP_ID: outVal[0] = data_.primaryAppId; return 1;
        case PARAM_HUB_EINK_APP_ID: outVal[0] = data_.einkAppId; return 1;
        default:
            if (paramId >= PARAM_APP_MOTOR_MODE_BASE && paramId < PARAM_APP_MOTOR_MODE_BASE + 24) {
                outVal[0] = data_.appMotorModes[paramId - PARAM_APP_MOTOR_MODE_BASE];
                return 1;
            }
            if (paramId >= PARAM_APP_EINK_SUB_BASE && paramId < PARAM_APP_EINK_SUB_BASE + 24) {
                outVal[0] = data_.appEinkSubs[paramId - PARAM_APP_EINK_SUB_BASE];
                return 1;
            }
            return 0;
    }
}

bool HubConfig::SetParam(uint16_t paramId, const uint8_t* value, uint8_t len)
{
    if (len < 1) return false;

    switch (paramId) {
        case PARAM_HUB_OLED_BRIGHTNESS: data_.oledBrightness = value[0]; break;
        case PARAM_HUB_STANDBY_TIMEOUT:
            if (len < 2) return false;
            data_.standbyTimeoutMs = value[0] | ((uint16_t)value[1] << 8);
            break;
        case PARAM_HUB_UART_ENABLED: data_.uartEnabled = value[0]; break;
        case PARAM_HUB_HEARTBEAT_SEC: data_.heartbeatSec = value[0]; break;
        case PARAM_HUB_TORQUE_LIMIT: data_.torqueLimit = value[0]; break;
        case PARAM_HUB_DEFAULT_PPR: data_.defaultPpr = value[0]; break;
        case PARAM_HUB_PRIMARY_APP_ID: data_.primaryAppId = value[0]; break;
        case PARAM_HUB_EINK_APP_ID: data_.einkAppId = value[0]; break;
        default:
            if (paramId >= PARAM_APP_MOTOR_MODE_BASE && paramId < PARAM_APP_MOTOR_MODE_BASE + 24) {
                data_.appMotorModes[paramId - PARAM_APP_MOTOR_MODE_BASE] = value[0];
                break;
            }
            if (paramId >= PARAM_APP_EINK_SUB_BASE && paramId < PARAM_APP_EINK_SUB_BASE + 24) {
                data_.appEinkSubs[paramId - PARAM_APP_EINK_SUB_BASE] = value[0];
                break;
            }
            return false;
    }

    dirty_ = true;
    return true;
}

void HubConfig::Save()
{
    if (!dirty_) return;
    // TODO: persist to Flash sector in Phase 5
    dirty_ = false;
}
