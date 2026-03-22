#ifndef APP_EINK_INFO_H
#define APP_EINK_INFO_H

#include "app/app_interface.h"

class AppEinkInfoPanel : public IApp {
public:
    uint8_t     GetId() const override { return 0x22; }
    const char* GetName() const override { return "Info"; }
    const uint8_t* GetIcon16() const override { return nullptr; }
    uint8_t GetFeatures() const override { return FEAT_EINK | FEAT_PC; }

    void OnEinkActivate() override { needsRefresh_ = true; }
    void OnEinkRender(EinkCanvas& canvas) override;
    bool NeedsEinkRefresh() const override { return needsRefresh_; }
    EinkRefreshMode GetRefreshMode() const override { return EINK_PARTIAL; }

    void OnPcData(uint8_t feedId, const uint8_t* data, uint8_t len) override;

private:
    char weatherLine_[32]{};
    char dateLine_[32]{};
    char calendarLine_[64]{};
    bool needsRefresh_ = true;

    static constexpr uint8_t FEED_ID_WEATHER = 0x20;
    static constexpr uint8_t FEED_ID_DATE    = 0x21;
    static constexpr uint8_t FEED_ID_CALENDAR= 0x22;
};

#endif // APP_EINK_INFO_H
