#include "app_eink_image.h"
#include <string.h>

void AppEinkImage::OnEinkRender(EinkCanvas& canvas)
{
    if (slotHasImage_[activeSlot_]) {
        memcpy(canvas.GetMutableBuffer(), imageData_, EinkCanvas::BUF_SIZE);
    } else {
        canvas.Clear(true);
        canvas.DrawText(20, 140, "No Image", true);
        canvas.DrawText(15, 155, "Upload from PC", true);
    }
    needsRefresh_ = false;
}

void AppEinkImage::OnPcData(uint8_t feedId, const uint8_t* data, uint8_t len)
{
    if (feedId != FEED_ID_EINK_IMAGE || len < 3) return;
    ReceiveImagePage(data[0], data[1], data + 2, len - 2);
}

void AppEinkImage::ReceiveImagePage(uint8_t slot, uint8_t page, const uint8_t* data, uint8_t len)
{
    if (slot >= MAX_SLOTS) return;
    uint32_t offset = (uint32_t)page * 128;
    if (offset + len > EinkCanvas::BUF_SIZE) return;

    memcpy(imageData_ + offset, data, len);
    receivedPages_++;

    if (receivedPages_ >= PAGES_PER_IMAGE) {
        slotHasImage_[slot] = true;
        receivedPages_ = 0;
        if (slot == activeSlot_) needsRefresh_ = true;
    }
}

const char* AppEinkImage::GetSubItemName(uint8_t idx) const
{
    static const char* names[] = {"Slot 1", "Slot 2", "Slot 3", "Slot 4"};
    return idx < MAX_SLOTS ? names[idx] : nullptr;
}

void AppEinkImage::OnSubItemSelected(uint8_t idx)
{
    if (idx < MAX_SLOTS) { activeSlot_ = idx; needsRefresh_ = true; }
}
