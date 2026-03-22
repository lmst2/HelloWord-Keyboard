#ifndef APP_EINK_IMAGE_H
#define APP_EINK_IMAGE_H

#include "app/app_interface.h"
#include "display/eink_canvas.h"

class AppEinkImage : public IApp {
public:
    uint8_t     GetId() const override { return 0x20; }
    const char* GetName() const override { return "Image"; }
    const uint8_t* GetIcon16() const override { return nullptr; }
    uint8_t GetFeatures() const override { return FEAT_EINK | FEAT_PC; }

    void OnEinkActivate() override { needsRefresh_ = true; }
    void OnEinkRender(EinkCanvas& canvas) override;
    bool NeedsEinkRefresh() const override { return needsRefresh_; }
    EinkRefreshMode GetRefreshMode() const override { return EINK_FULL; }

    void OnPcData(uint8_t feedId, const uint8_t* data, uint8_t len) override;

    // Paged image upload: slot, page index, 128 bytes per page
    void ReceiveImagePage(uint8_t slot, uint8_t page, const uint8_t* data, uint8_t len);

    uint8_t GetSubItemCount() const override { return MAX_SLOTS; }
    const char* GetSubItemName(uint8_t idx) const override;
    uint8_t GetActiveSubItem() const override { return activeSlot_; }
    void OnSubItemSelected(uint8_t idx) override;

private:
    static constexpr uint8_t MAX_SLOTS = 4;
    static constexpr uint16_t PAGES_PER_IMAGE = (EinkCanvas::BUF_SIZE + 127) / 128;

    uint8_t imageData_[EinkCanvas::BUF_SIZE]{};
    bool slotHasImage_[MAX_SLOTS]{};
    uint8_t activeSlot_ = 0;
    bool needsRefresh_ = true;
    uint16_t receivedPages_ = 0;

    static constexpr uint8_t FEED_ID_EINK_IMAGE = 0x10;
};

#endif // APP_EINK_IMAGE_H
