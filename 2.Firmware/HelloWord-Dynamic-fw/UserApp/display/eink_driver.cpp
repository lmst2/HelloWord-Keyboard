#include "eink_driver.h"
#include <string.h>

EinkDriver einkDriver;

void EinkDriver::Init(Eink290BW* hw)
{
    hw_ = hw;
    initialized_ = true;
}

void EinkDriver::FullRefresh(const uint8_t* framebuffer)
{
    if (!hw_ || !initialized_) return;
    busy_ = true;
    memcpy(Eink290BW::buffer, framebuffer, BUF_SIZE);
    hw_->DrawBitmap(Eink290BW::buffer);
    hw_->Update();
    busy_ = false;
}

void EinkDriver::PartialRefresh(const uint8_t* framebuffer)
{
    // For now partial refresh = full refresh
    // TODO: implement true partial LUT when porting SSD16xx from ZMK
    FullRefresh(framebuffer);
}
