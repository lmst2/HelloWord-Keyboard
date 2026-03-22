#ifndef APP_EINK_QUOTE_H
#define APP_EINK_QUOTE_H

#include "app/app_interface.h"

class AppEinkQuote : public IApp {
public:
    uint8_t     GetId() const override { return 0x26; }
    const char* GetName() const override { return "Quote"; }
    const uint8_t* GetIcon16() const override { return nullptr; }
    uint8_t GetFeatures() const override { return FEAT_EINK | FEAT_PC; }

    void OnEinkActivate() override { needsRefresh_ = true; }
    void OnEinkRender(EinkCanvas& canvas) override;
    bool NeedsEinkRefresh() const override { return needsRefresh_; }
    EinkRefreshMode GetRefreshMode() const override { return EINK_FULL; }

    void OnPcData(uint8_t feedId, const uint8_t* data, uint8_t len) override;

private:
    char quoteBuf_[200]{};
    char authorBuf_[40]{};
    bool needsRefresh_ = true;

    void WordWrapDraw(EinkCanvas& canvas, int16_t x, int16_t y, uint16_t maxW, const char* text);

    static constexpr uint8_t FEED_ID_QUOTE  = 0x50;
    static constexpr uint8_t FEED_ID_AUTHOR = 0x51;
};

#endif // APP_EINK_QUOTE_H
