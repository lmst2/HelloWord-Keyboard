#include "display_manager.h"

DisplayManager displayManager;

void DisplayManager::Init(EinkDriver* einkDrv, OledDisplay* oledDisp, AppManager* mgr)
{
    einkDrv_ = einkDrv;
    oledDisp_ = oledDisp;
    mgr_ = mgr;
    canvas_.Clear(true);
}

void DisplayManager::TickOled(uint32_t nowMs)
{
    if (oledDisp_) oledDisp_->Render(nowMs);
}

void DisplayManager::TickEink(uint32_t nowMs)
{
    if (!einkDrv_ || !mgr_) return;
    if (einkDrv_->IsBusy()) return;

    if (nowMs - lastEinkRefreshMs_ < EINK_MIN_REFRESH_INTERVAL_MS && !einkFirstRender_)
        return;

    IApp* einkApp = mgr_->GetEinkApp();
    if (!einkApp) return;
    if (!(einkApp->GetFeatures() & IApp::FEAT_EINK)) return;

    if (!einkApp->NeedsEinkRefresh() && !einkFirstRender_)
        return;

    canvas_.Clear(true);
    einkApp->OnEinkRender(canvas_);

    if (einkApp->GetRefreshMode() == EINK_FULL || einkFirstRender_) {
        einkDrv_->FullRefresh(canvas_.GetBuffer());
    } else {
        einkDrv_->PartialRefresh(canvas_.GetBuffer());
    }

    canvas_.ClearDirty();
    lastEinkRefreshMs_ = nowMs;
    einkFirstRender_ = false;
}
