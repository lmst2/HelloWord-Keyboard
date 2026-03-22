#ifndef APP_EINK_SCROLL_H
#define APP_EINK_SCROLL_H

#include "app/app_interface.h"

class AppEinkScrollText : public IApp {
public:
    uint8_t     GetId() const override { return 0x23; }
    const char* GetName() const override { return "Scroll"; }
    const uint8_t* GetIcon16() const override { return nullptr; }
    uint8_t GetFeatures() const override { return FEAT_EINK | FEAT_PC; }

    void OnEinkActivate() override { needsRefresh_ = true; }
    void OnEinkRender(EinkCanvas& canvas) override;
    bool NeedsEinkRefresh() const override { return needsRefresh_; }
    EinkRefreshMode GetRefreshMode() const override { return EINK_PARTIAL; }
    void OnTick(uint32_t nowMs) override;

    void OnPcData(uint8_t feedId, const uint8_t* data, uint8_t len) override;

    uint8_t GetSubItemCount() const override { return 3; }
    const char* GetSubItemName(uint8_t idx) const override;
    uint8_t GetActiveSubItem() const override { return speed_; }
    void OnSubItemSelected(uint8_t idx) override;

private:
    char textBuf_[256]{};
    uint16_t textLen_ = 0;
    int16_t scrollOffset_ = 0;
    uint8_t speed_ = 1;
    bool needsRefresh_ = false;
    uint32_t lastScrollMs_ = 0;

    static constexpr uint8_t FEED_ID_TEXT = 0x30;
    static constexpr uint32_t SCROLL_INTERVALS_MS[] = {500, 200, 100};
};

#endif // APP_EINK_SCROLL_H
