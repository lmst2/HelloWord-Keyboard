use crate::dfu::{FirmwareInfo, FlashProgress};
use crate::dfu::DfuService;
use crate::state::SharedState;
use tauri::State;

#[tauri::command]
pub async fn dfu_get_info(state: State<'_, SharedState>) -> Result<FirmwareInfo, String> {
    let s = state.inner().read().await;
    let mut dm = s.device_mgr.lock().await;
    let msg = dm.hub_request_fw_info()?;
    Ok(DfuService::parse_fw_info(&msg.payload))
}

#[tauri::command]
pub async fn dfu_flash_keyboard(
    state: State<'_, SharedState>,
    firmware_bytes: Vec<u8>,
) -> Result<(), String> {
    let app = state.inner().clone();
    {
        let s = app.read().await;
        s.dfu_svc.reset_progress();
    }
    let fw = firmware_bytes;
    tokio::task::spawn_blocking(move || {
        let s = app.blocking_read();
        let mut dm = s.device_mgr.blocking_lock();
        s.dfu_svc.flash_keyboard(&mut *dm, &fw)
    })
    .await
    .map_err(|e| format!("Flash task failed: {e}"))?
}

#[tauri::command]
pub async fn dfu_flash_hub(
    state: State<'_, SharedState>,
    firmware_bytes: Vec<u8>,
) -> Result<(), String> {
    let app = state.inner().clone();
    {
        let s = app.read().await;
        s.dfu_svc.reset_progress();
    }
    let fw = firmware_bytes;
    tokio::task::spawn_blocking(move || {
        let s = app.blocking_read();
        let mut dm = s.device_mgr.blocking_lock();
        s.dfu_svc.flash_hub(&mut *dm, &fw)
    })
    .await
    .map_err(|e| format!("Flash task failed: {e}"))?
}

#[tauri::command]
pub async fn dfu_get_progress(state: State<'_, SharedState>) -> Result<FlashProgress, String> {
    let s = state.inner().read().await;
    Ok(s.dfu_svc.get_progress())
}
