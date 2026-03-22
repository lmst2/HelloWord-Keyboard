#ifndef APP_EINK_STATS_H
#define APP_EINK_STATS_H

#include "app/app_interface.h"

class AppEinkStats : public IApp {
public:
    uint8_t     GetId() const override { return 0x21; }
    const char* GetName() const override { return "Stats"; }
    const uint8_t* GetIcon16() const override { return nullptr; }
    uint8_t GetFeatures() const override { return FEAT_EINK | FEAT_PC; }

    void OnEinkActivate() override { needsRefresh_ = true; }
    void OnEinkRender(EinkCanvas& canvas) override;
    bool NeedsEinkRefresh() const override { return needsRefresh_; }
    EinkRefreshMode GetRefreshMode() const override { return EINK_PARTIAL; }

    void OnPcData(uint8_t feedId, const uint8_t* data, uint8_t len) override;

    uint8_t GetSubItemCount() const override { return 3; }
    const char* GetSubItemName(uint8_t idx) const override;
    uint8_t GetActiveSubItem() const override { return layoutStyle_; }
    void OnSubItemSelected(uint8_t idx) override;

private:
    uint8_t cpuPercent_ = 0;
    uint8_t ramPercent_ = 0;
    uint8_t gpuPercent_ = 0;
    int8_t cpuTemp_ = 0;
    int8_t gpuTemp_ = 0;
    uint8_t layoutStyle_ = 0;
    bool needsRefresh_ = true;

    static constexpr uint8_t FEED_ID_CPU = 0x01;
    static constexpr uint8_t FEED_ID_RAM = 0x02;
    static constexpr uint8_t FEED_ID_GPU = 0x03;
    static constexpr uint8_t FEED_ID_TEMPS = 0x04;
};

#endif // APP_EINK_STATS_H
