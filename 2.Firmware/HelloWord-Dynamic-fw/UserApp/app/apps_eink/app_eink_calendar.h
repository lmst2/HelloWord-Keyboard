#ifndef APP_EINK_CALENDAR_H
#define APP_EINK_CALENDAR_H

#include "app/app_interface.h"

class AppEinkCalendar : public IApp {
public:
    uint8_t     GetId() const override { return 0x25; }
    const char* GetName() const override { return "Cal"; }
    const uint8_t* GetIcon16() const override { return nullptr; }
    uint8_t GetFeatures() const override { return FEAT_EINK | FEAT_PC; }

    void OnEinkActivate() override { needsRefresh_ = true; }
    void OnEinkRender(EinkCanvas& canvas) override;
    bool NeedsEinkRefresh() const override { return needsRefresh_; }
    EinkRefreshMode GetRefreshMode() const override { return EINK_FULL; }

    void OnPcData(uint8_t feedId, const uint8_t* data, uint8_t len) override;

private:
    uint16_t year_ = 2026;
    uint8_t month_ = 1;
    uint8_t day_ = 1;
    uint8_t weekday_ = 0;
    bool needsRefresh_ = true;

    uint8_t DaysInMonth(uint16_t y, uint8_t m) const;
    uint8_t DayOfWeek(uint16_t y, uint8_t m, uint8_t d) const;

    static constexpr uint8_t FEED_ID_DATE_INFO = 0x40;
};

#endif // APP_EINK_CALENDAR_H
