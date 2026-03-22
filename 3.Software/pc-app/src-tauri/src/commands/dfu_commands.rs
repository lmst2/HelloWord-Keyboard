use crate::dfu::{FirmwareInfo, FlashProgress};
use crate::dfu::DfuService;
use crate::state::SharedState;
use tauri::State;

#[tauri::command]
pub async fn dfu_get_info(state: State<'_, SharedState>) -> Result<FirmwareInfo, String> {
    log::info!("dfu_get_info: querying Hub firmware info");
    let s = state.inner().read().await;
    let mut dm = s.device_mgr.lock().await;
    let msg = dm.hub_request_fw_info()?;
    let info = DfuService::parse_fw_info(&msg.payload);
    log::info!(
        "dfu_get_info: kb_fw={:?} hub_fw={:?}",
        info.kb_version,
        info.hub_version
    );
    Ok(info)
}

#[tauri::command]
pub async fn dfu_flash_keyboard(
    state: State<'_, SharedState>,
    firmware_bytes: Vec<u8>,
) -> Result<(), String> {
    log::info!(
        "dfu_flash_keyboard: firmware_bytes={}",
        firmware_bytes.len()
    );
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
    .map_err(|e| format!("Flash task failed: {e}"))??;
    log::info!("dfu_flash_keyboard: completed");
    Ok(())
}

#[tauri::command]
pub async fn dfu_flash_hub(
    state: State<'_, SharedState>,
    firmware_bytes: Vec<u8>,
) -> Result<(), String> {
    log::info!("dfu_flash_hub: firmware_bytes={}", firmware_bytes.len());
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
    .map_err(|e| format!("Flash task failed: {e}"))??;
    log::info!("dfu_flash_hub: completed");
    Ok(())
}

#[tauri::command]
pub async fn dfu_get_progress(state: State<'_, SharedState>) -> Result<FlashProgress, String> {
    let s = state.inner().read().await;
    Ok(s.dfu_svc.get_progress())
}
