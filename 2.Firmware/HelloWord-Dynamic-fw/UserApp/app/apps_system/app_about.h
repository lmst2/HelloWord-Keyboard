#ifndef APP_ABOUT_H
#define APP_ABOUT_H

#include "app/app_interface.h"

class AppAbout : public IApp {
public:
    uint8_t     GetId() const override { return 0xFE; }
    const char* GetName() const override { return "About"; }
    const uint8_t* GetIcon16() const override { return nullptr; }
    uint8_t GetFeatures() const override { return FEAT_EINK; }

    void OnEinkRender(EinkCanvas& canvas) override;
    bool NeedsEinkRefresh() const override { return needsRefresh_; }
    void OnEinkActivate() override { needsRefresh_ = true; }

private:
    bool needsRefresh_ = true;
};

#endif // APP_ABOUT_H
