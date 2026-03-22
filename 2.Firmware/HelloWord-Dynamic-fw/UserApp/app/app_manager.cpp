#include "app_manager.h"

AppManager appManager;

bool AppManager::IsVisible(IApp* app) const
{
    if (!app) return false;
    uint8_t support = app->GetOsSupport();
    return support == OS_BOTH || support == osMode_;
}

void AppManager::RegisterApp(IApp* app)
{
    if (appCount_ < 24 && app)
        apps_[appCount_++] = app;
}

int16_t AppManager::FindVisibleIndex(uint8_t rawIdx) const
{
    int16_t vis = -1;
    for (uint8_t i = 0; i <= rawIdx && i < appCount_; i++) {
        if (IsVisible(apps_[i])) vis++;
    }
    return vis;
}

int16_t AppManager::NextVisibleFrom(uint8_t rawIdx, int8_t direction) const
{
    for (uint8_t i = 1; i < appCount_; i++) {
        int16_t idx = ((int16_t)rawIdx + direction * (int16_t)i + appCount_) % appCount_;
        if (IsVisible(apps_[idx])) return idx;
    }
    return rawIdx;
}

// ---- Primary app ----
void AppManager::SetPrimaryApp(uint8_t appId)
{
    for (uint8_t i = 0; i < appCount_; i++) {
        if (apps_[i]->GetId() == appId) {
            if (i != primaryIdx_) {
                apps_[primaryIdx_]->OnDeactivate();
                primaryIdx_ = i;
                apps_[primaryIdx_]->OnActivate();
            }
            return;
        }
    }
}

void AppManager::NextPrimaryApp()
{
    if (inSubmenu_) { NextSubItem(); return; }
    int16_t next = NextVisibleFrom(primaryIdx_, 1);
    if (next >= 0 && next != primaryIdx_) {
        apps_[primaryIdx_]->OnDeactivate();
        primaryIdx_ = (uint8_t)next;
        apps_[primaryIdx_]->OnActivate();
    }
}

void AppManager::PrevPrimaryApp()
{
    if (inSubmenu_) { PrevSubItem(); return; }
    int16_t prev = NextVisibleFrom(primaryIdx_, -1);
    if (prev >= 0 && prev != primaryIdx_) {
        apps_[primaryIdx_]->OnDeactivate();
        primaryIdx_ = (uint8_t)prev;
        apps_[primaryIdx_]->OnActivate();
    }
}

IApp* AppManager::GetPrimaryApp() const
{
    return primaryIdx_ < appCount_ ? apps_[primaryIdx_] : nullptr;
}

// ---- E-Ink app ----
void AppManager::SetEinkApp(uint8_t appId)
{
    for (uint8_t i = 0; i < appCount_; i++) {
        if (apps_[i]->GetId() == appId && (apps_[i]->GetFeatures() & IApp::FEAT_EINK)) {
            if (i != einkIdx_) {
                if (einkIdx_ < appCount_) apps_[einkIdx_]->OnEinkDeactivate();
                einkIdx_ = i;
                apps_[einkIdx_]->OnEinkActivate();
            }
            return;
        }
    }
}

void AppManager::NextEinkApp()
{
    for (uint8_t i = 1; i < appCount_; i++) {
        uint8_t idx = (einkIdx_ + i) % appCount_;
        if ((apps_[idx]->GetFeatures() & IApp::FEAT_EINK) && IsVisible(apps_[idx])) {
            if (einkIdx_ < appCount_) apps_[einkIdx_]->OnEinkDeactivate();
            einkIdx_ = idx;
            apps_[einkIdx_]->OnEinkActivate();
            return;
        }
    }
}

void AppManager::PrevEinkApp()
{
    for (uint8_t i = 1; i < appCount_; i++) {
        uint8_t idx = (einkIdx_ + appCount_ - i) % appCount_;
        if ((apps_[idx]->GetFeatures() & IApp::FEAT_EINK) && IsVisible(apps_[idx])) {
            if (einkIdx_ < appCount_) apps_[einkIdx_]->OnEinkDeactivate();
            einkIdx_ = idx;
            apps_[einkIdx_]->OnEinkActivate();
            return;
        }
    }
}

IApp* AppManager::GetEinkApp() const
{
    return einkIdx_ < appCount_ ? apps_[einkIdx_] : nullptr;
}

// ---- L2 submenu ----
void AppManager::EnterSubmenu()
{
    IApp* app = GetPrimaryApp();
    if (!app || app->GetSubItemCount() == 0) return;
    inSubmenu_ = true;
    submenuTarget_ = 0;
    subItemCursor_ = app->GetActiveSubItem();
}

void AppManager::ExitSubmenu()
{
    if (!inSubmenu_) return;
    IApp* app = GetPrimaryApp();
    if (app) app->OnSubItemSelected(subItemCursor_);
    inSubmenu_ = false;
}

void AppManager::NextSubItem()
{
    IApp* app = GetPrimaryApp();
    if (!app) return;
    uint8_t count = app->GetSubItemCount();
    if (count == 0) return;
    subItemCursor_ = (subItemCursor_ + 1) % count;
}

void AppManager::PrevSubItem()
{
    IApp* app = GetPrimaryApp();
    if (!app) return;
    uint8_t count = app->GetSubItemCount();
    if (count == 0) return;
    subItemCursor_ = (subItemCursor_ + count - 1) % count;
}

// ---- OS filtering ----
void AppManager::SetOsMode(uint8_t os)
{
    osMode_ = os;
}

uint8_t AppManager::GetVisibleAppCount() const
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < appCount_; i++)
        if (IsVisible(apps_[i])) count++;
    return count;
}

IApp* AppManager::GetVisibleApp(uint8_t visibleIdx) const
{
    uint8_t vis = 0;
    for (uint8_t i = 0; i < appCount_; i++) {
        if (IsVisible(apps_[i])) {
            if (vis == visibleIdx) return apps_[i];
            vis++;
        }
    }
    return nullptr;
}

uint8_t AppManager::GetVisiblePrimaryIndex() const
{
    uint8_t vis = 0;
    for (uint8_t i = 0; i < appCount_; i++) {
        if (i == primaryIdx_) return vis;
        if (IsVisible(apps_[i])) vis++;
    }
    return 0;
}

// ---- Dispatch ----
void AppManager::OnKnobDelta(int32_t delta)
{
    IApp* app = GetPrimaryApp();
    if (app) app->OnKnobDelta(delta);
}

void AppManager::OnPcData(uint8_t feedId, const uint8_t* data, uint8_t len)
{
    for (uint8_t i = 0; i < appCount_; i++) {
        if (apps_[i]->GetFeatures() & IApp::FEAT_PC)
            apps_[i]->OnPcData(feedId, data, len);
    }
}

void AppManager::Tick(uint32_t nowMs)
{
    for (uint8_t i = 0; i < appCount_; i++)
        apps_[i]->OnTick(nowMs);
}
