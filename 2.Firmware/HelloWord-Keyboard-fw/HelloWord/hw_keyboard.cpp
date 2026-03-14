#include <cstring>
#include "hw_keyboard.h"


const uint8_t HWKeyboard::BRIGHTNESS_MAP[] = {7, 5, 4, 3, 2, 1, 0};


inline void DelayUs(uint32_t _us)
{
    for (int i = 0; i < _us; i++)
        for (int j = 0; j < 8; j++)  // ToDo: tune this for different chips
            __NOP();
}


uint8_t* HWKeyboard::ScanKeyStates()
{
    memset(spiBuffer, 0xFF, IO_NUMBER / 8 + 1);
    PL_GPIO_Port->BSRR = PL_Pin; // Latch

    spiHandle->pRxBuffPtr = (uint8_t*) spiBuffer;
    spiHandle->RxXferCount = IO_NUMBER / 8 + 1;
    __HAL_SPI_ENABLE(spiHandle);
    while (spiHandle->RxXferCount > 0U)
    {
        if (__HAL_SPI_GET_FLAG(spiHandle, SPI_FLAG_RXNE))
        {
            /* read the received data */
            (*(uint8_t*) spiHandle->pRxBuffPtr) = *(__IO uint8_t*) &spiHandle->Instance->DR;
            spiHandle->pRxBuffPtr += sizeof(uint8_t);
            spiHandle->RxXferCount--;
        }
    }
    __HAL_SPI_DISABLE(spiHandle);

    PL_GPIO_Port->BRR = PL_Pin; // Sample
    return scanBuffer;
}


void HWKeyboard::ApplyDebounceFilter(uint32_t _filterTimeUs)
{
    memcpy(debounceBuffer, spiBuffer, IO_NUMBER / 8 + 1);

    DelayUs(_filterTimeUs);
    ScanKeyStates();

    uint8_t mask;
    for (int i = 0; i < IO_NUMBER / 8 + 1; i++)
    {
        mask = debounceBuffer[i] ^ spiBuffer[i];
        spiBuffer[i] |= mask;
    }
}


void HWKeyboard::ApplyKeyDebounce(uint8_t _cycles)
{
    static uint8_t stableState[IO_NUMBER / 8 + 1];
    static uint8_t counter[IO_NUMBER / 8 + 1] = {0};
    static bool init = false;

    if (!init)
    {
        memcpy(stableState, spiBuffer, IO_NUMBER / 8 + 1);
        init = true;
    }

    for (uint8_t i = 0; i < IO_NUMBER / 8 + 1; i++)
    {
        if (spiBuffer[i] != stableState[i])
        {
            counter[i]++;
            if (counter[i] >= _cycles)
            {
                stableState[i] = spiBuffer[i];
                counter[i] = 0;
            }
        } else
        {
            counter[i] = 0;
        }
    }

    memcpy(spiBuffer, stableState, IO_NUMBER / 8 + 1);
}


uint8_t* HWKeyboard::Remap(uint8_t _layer)
{
    int16_t index, bitIndex;

    memset(remapBuffer, 0, IO_NUMBER / 8);
    for (int16_t i = 0; i < IO_NUMBER / 8; i++)
    {
        for (int16_t j = 0; j < 8; j++)
        {
            index = (int16_t) (keyMap[0][i * 8 + j] / 8);
            bitIndex = (int16_t) (keyMap[0][i * 8 + j] % 8);
            if (scanBuffer[index] & (0x80 >> bitIndex))
                remapBuffer[i] |= 0x80 >> j;
        }
        remapBuffer[i] = ~remapBuffer[i];
    }

    memset(hidBuffer, 0, KEY_REPORT_SIZE);

    int i = 0, j = 0;
    while (8 * i + j < IO_NUMBER - 6)
    {
        for (j = 0; j < 8; j++)
        {
            index = (int16_t) (keyMap[_layer][i * 8 + j] / 8 + 1); // +1 for modifier
            bitIndex = (int16_t) (keyMap[_layer][i * 8 + j] % 8);
            if (bitIndex < 0)
            {
                index -= 1;
                bitIndex += 8;
            } else if (index > 100)
                continue;

            if (remapBuffer[i] & (0x80 >> j))
                hidBuffer[index + 1] |= 1 << (bitIndex); // +1 for Report-ID
        }
        i++;
        j = 0;
    }

    return hidBuffer;
}


bool HWKeyboard::FnPressed()
{
    return remapBuffer[9] & 0x04;
}


void HWKeyboard::EncodeRgbBufferByID(uint8_t _keyId, HWKeyboard::Color_t _color, float _brightness)
{
    if (_brightness <= 0.0f || (_color.r == 0 && _color.g == 0 && _color.b == 0))
    {
        TurnOffRgbOutputByID(_keyId);
        return;
    }

    // Keep the legacy ws2812 workaround for lit LEDs, but bypass it for a true off state.
    if (_color.b < 1) _color.b = 1;

    const uint8_t green = (uint8_t) ((float) _color.g * _brightness) >> brightnessPreDiv;
    const uint8_t red = (uint8_t) ((float) _color.r * _brightness) >> brightnessPreDiv;
    const uint8_t blue = (uint8_t) ((float) _color.b * _brightness) >> brightnessPreDiv;

    for (int i = 0; i < 8; i++)
    {
        rgbBuffer[_keyId][0][i] = (green & (0x80 >> i)) ? WS_HIGH : WS_LOW;
        rgbBuffer[_keyId][1][i] = (red & (0x80 >> i)) ? WS_HIGH : WS_LOW;
        rgbBuffer[_keyId][2][i] = (blue & (0x80 >> i)) ? WS_HIGH : WS_LOW;
    }
}


void HWKeyboard::SetRgbBufferByID(uint8_t _keyId, HWKeyboard::Color_t _color, float _brightness)
{
    ledColors[_keyId] = _color;
    EncodeRgbBufferByID(_keyId, _color, _brightness);
}


void HWKeyboard::ApplyStoredRgbByID(uint8_t _keyId, float _brightness)
{
    EncodeRgbBufferByID(_keyId, ledColors[_keyId], _brightness);
}


void HWKeyboard::TurnOffRgbOutputByID(uint8_t _keyId)
{
    for (uint8_t channel = 0; channel < 3; channel++)
    {
        for (uint8_t bit = 0; bit < 8; bit++)
            rgbBuffer[_keyId][channel][bit] = WS_LOW;
    }
}


void HWKeyboard::SyncLights()
{
    while (isRgbTxBusy);
    isRgbTxBusy = true;
    HAL_SPI_Transmit_DMA(&hspi2, (uint8_t*) rgbBuffer, LED_NUMBER * 3 * 8);
    while (isRgbTxBusy);
    isRgbTxBusy = true;
    HAL_SPI_Transmit_DMA(&hspi2, wsCommit, 64);
}


uint8_t* HWKeyboard::GetHidReportBuffer(uint8_t _reportId)
{
    switch (_reportId)
    {
        case 1:
            hidBuffer[0] = 1;
            return hidBuffer;
        case 2:
            hidBuffer[KEY_REPORT_SIZE] = 2;
            return hidBuffer + KEY_REPORT_SIZE;
        case 3:
            hidBuffer[KEY_REPORT_SIZE + RAW_REPORT_SIZE] = 3;
            return hidBuffer + KEY_REPORT_SIZE + RAW_REPORT_SIZE;
        default:
            return hidBuffer;
    }
}


bool HWKeyboard::KeyPressed(KeyCode_t _key)
{
    int index, bitIndex;

    if (_key < RESERVED)
    {
        index = 0;
        bitIndex = (int) _key + 8;
    } else
    {
        index = _key / 8 + 1;
        bitIndex = _key % 8;
    }

    return hidBuffer[index + 1] & (1 << bitIndex);
}


void HWKeyboard::Press(HWKeyboard::KeyCode_t _key)
{
    int index, bitIndex;

    if (_key < RESERVED)
    {
        index = 0;
        bitIndex = (int) _key + 8;
    } else
    {
        index = _key / 8 + 1;
        bitIndex = _key % 8;
    }

    hidBuffer[index + 1] |= (1 << bitIndex);
}


void HWKeyboard::Release(HWKeyboard::KeyCode_t _key)
{
    int index, bitIndex;

    if (_key < RESERVED)
    {
        index = 0;
        bitIndex = (int) _key + 8;
    } else
    {
        index = _key / 8 + 1;
        bitIndex = _key % 8;
    }

    hidBuffer[index + 1] &= ~(1 << bitIndex);
}


uint8_t HWKeyboard::GetTouchBarState(uint8_t _id)
{
    // Raw touch bits are stored from low to high; export them as logical
    // left-to-right positions in the same physical order.
    const uint8_t rawState = remapBuffer[10] & 0b00111111;
    uint8_t tmp = 0;

    for (uint8_t i = 0; i < TOUCHPAD_NUMBER; i++)
    {
        if (rawState & (1U << i))
            tmp |= (uint8_t) (1U << (TOUCHPAD_NUMBER - 1U - i));
    }

    return _id == 0 ? tmp : (tmp & (1 << (_id - 1)));
}


void HWKeyboard::SetMouseWheel(int8_t _wheel)
{
    uint8_t* report = GetHidReportBuffer(3);
    memset(report + 1, 0, MOUSE_REPORT_SIZE - 1);
    report[4] = (uint8_t) _wheel;
}


void HWKeyboard::ClearMouseReport()
{
    uint8_t* report = GetHidReportBuffer(3);
    memset(report + 1, 0, MOUSE_REPORT_SIZE - 1);
}


void HWKeyboard::IncreaseBrightness()
{
    if (brightnessLevel < BRIGHTNESS_LEVELS - 1)
    {
        brightnessLevel++;
        brightnessPreDiv = BRIGHTNESS_MAP[brightnessLevel];
    }
}


void HWKeyboard::DecreaseBrightness()
{
    if (brightnessLevel > 0)
    {
        brightnessLevel--;
        brightnessPreDiv = BRIGHTNESS_MAP[brightnessLevel];
    }
}


void HWKeyboard::SetEffect(LightEffect_t _effect)
{
    if (_effect < EFFECT_COUNT)
        currentEffect = _effect;
}


void HWKeyboard::NextEffect()
{
    currentEffect = (LightEffect_t) ((currentEffect + 1) % EFFECT_COUNT);
}


void HWKeyboard::UpdateKeyPressState()
{
    static uint8_t stableCount[KEY_NUMBER] = {0};

    for (uint8_t i = 0; i < KEY_NUMBER; i++)
    {
        if (remapBuffer[i / 8] & (0x80 >> (i % 8)))
        {
            if (stableCount[i] < 255) stableCount[i]++;
            if (stableCount[i] >= 3)
            {
                uint8_t led = i;
                if (i < 14)                    led = 13 - i;
                else if (i >= 29 && i < 44)    led = 72 - i;
                else if (i >= 58 && i < 72)    led = 129 - i;
                keyBrightness[led] = 255;
            }
        }
        else
        {
            stableCount[i] = 0;
        }
    }
}


bool HWKeyboard::HasAnyPhysicalInput() const
{
    for (uint8_t i = 0; i < IO_NUMBER / 8; i++)
    {
        if (remapBuffer[i] != 0)
            return true;
    }

    return false;
}


