use crate::dfu::dfu_service::{FirmwareInfo, FlashProgress};
use crate::dfu::DfuService;
use crate::state::SharedState;
use tauri::State;

#[tauri::command]
pub async fn dfu_get_info(state: State<'_, SharedState>) -> Result<FirmwareInfo, String> {
    let s = state.read().await;
    let mut dm = s.device_mgr.write().await;
    let msg = dm.hub_request_fw_info()?;
    Ok(DfuService::parse_fw_info(&msg.payload))
}

#[tauri::command]
pub async fn dfu_flash_keyboard(
    state: State<'_, SharedState>,
    firmware_bytes: Vec<u8>,
) -> Result<(), String> {
    let s = state.read().await;
    let mut dm = s.device_mgr.write().await;
    s.dfu_svc.reset_progress();
    s.dfu_svc.flash_keyboard(&mut dm, &firmware_bytes)
}

#[tauri::command]
pub async fn dfu_flash_hub(
    state: State<'_, SharedState>,
    firmware_bytes: Vec<u8>,
) -> Result<(), String> {
    let s = state.read().await;
    let mut dm = s.device_mgr.write().await;
    s.dfu_svc.reset_progress();
    s.dfu_svc.flash_hub(&mut dm, &firmware_bytes)
}

#[tauri::command]
pub async fn dfu_get_progress(state: State<'_, SharedState>) -> Result<FlashProgress, String> {
    let s = state.read().await;
    Ok(s.dfu_svc.get_progress())
}
