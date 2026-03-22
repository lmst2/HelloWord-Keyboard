#include "app_eink_scroll.h"
#include "display/eink_canvas.h"
#include <string.h>

constexpr uint32_t AppEinkScrollText::SCROLL_INTERVALS_MS[];

void AppEinkScrollText::OnPcData(uint8_t feedId, const uint8_t* data, uint8_t len)
{
    if (feedId != FEED_ID_TEXT || len == 0) return;
    uint16_t copyLen = len < 255 ? len : 255;
    memcpy(textBuf_, data, copyLen);
    textBuf_[copyLen] = '\0';
    textLen_ = copyLen;
    scrollOffset_ = 0;
    needsRefresh_ = true;
}

void AppEinkScrollText::OnTick(uint32_t nowMs)
{
    if (textLen_ == 0) return;
    uint32_t interval = speed_ < 3 ? SCROLL_INTERVALS_MS[speed_] : 200;
    if (nowMs - lastScrollMs_ >= interval) {
        lastScrollMs_ = nowMs;
        scrollOffset_++;
        int16_t totalWidth = (int16_t)textLen_ * 6;
        if (scrollOffset_ > totalWidth + EinkCanvas::WIDTH)
            scrollOffset_ = -(int16_t)EinkCanvas::WIDTH;
        needsRefresh_ = true;
    }
}

void AppEinkScrollText::OnEinkRender(EinkCanvas& canvas)
{
    canvas.Clear(true);
    canvas.DrawText(10, 5, "Scroll Text", true);
    canvas.DrawHLine(0, 14, 128, true);

    // Draw scrolling text at center vertical position
    int16_t textX = (int16_t)EinkCanvas::WIDTH / 2 - scrollOffset_;
    canvas.DrawText(textX, 140, textBuf_, true);

    needsRefresh_ = false;
}

const char* AppEinkScrollText::GetSubItemName(uint8_t idx) const
{
    static const char* names[] = {"Slow", "Medium", "Fast"};
    return idx < 3 ? names[idx] : nullptr;
}

void AppEinkScrollText::OnSubItemSelected(uint8_t idx)
{
    if (idx < 3) speed_ = idx;
}
