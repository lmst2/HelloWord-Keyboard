#include "app_eink_quote.h"
#include "display/eink_canvas.h"
#include <string.h>

void AppEinkQuote::OnPcData(uint8_t feedId, const uint8_t* data, uint8_t len)
{
    if (feedId == FEED_ID_QUOTE && len > 0) {
        uint8_t n = len < 199 ? len : 199;
        memcpy(quoteBuf_, data, n);
        quoteBuf_[n] = '\0';
        needsRefresh_ = true;
    } else if (feedId == FEED_ID_AUTHOR && len > 0) {
        uint8_t n = len < 39 ? len : 39;
        memcpy(authorBuf_, data, n);
        authorBuf_[n] = '\0';
        needsRefresh_ = true;
    }
}

void AppEinkQuote::WordWrapDraw(EinkCanvas& canvas, int16_t x, int16_t y, uint16_t maxW, const char* text)
{
    int16_t cx = x, cy = y;
    uint16_t charW = 6, lineH = 10;

    while (*text) {
        if (*text == '\n' || (cx + charW > x + (int16_t)maxW)) {
            cx = x;
            cy += lineH;
            if (*text == '\n') { text++; continue; }
        }
        canvas.DrawChar(cx, cy, *text, true);
        cx += charW;
        text++;
    }
}

void AppEinkQuote::OnEinkRender(EinkCanvas& canvas)
{
    canvas.Clear(true);

    // Decorative borders
    canvas.DrawRect(4, 4, 120, EinkCanvas::HEIGHT - 8, false, true);
    canvas.DrawRect(6, 6, 116, EinkCanvas::HEIGHT - 12, false, true);

    // Opening quotation mark
    canvas.DrawText(12, 20, "\"", true);

    // Quote body with word wrap
    if (quoteBuf_[0]) {
        WordWrapDraw(canvas, 12, 34, 104, quoteBuf_);
    } else {
        canvas.DrawText(20, 120, "Waiting for", true);
        canvas.DrawText(25, 135, "quote from PC", true);
    }

    // Author
    if (authorBuf_[0]) {
        int16_t authorY = EinkCanvas::HEIGHT - 40;
        canvas.DrawText(60, authorY, "-- ", true);
        canvas.DrawText(78, authorY, authorBuf_, true);
    }

    needsRefresh_ = false;
}
