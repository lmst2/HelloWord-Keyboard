use crate::device::device_manager::DeviceStatus;
use crate::state::SharedState;
use tauri::State;

#[tauri::command]
pub async fn get_device_status(state: State<'_, SharedState>) -> Result<DeviceStatus, String> {
    let s = state.read().await;
    let dm = s.device_mgr.read().await;
    Ok(dm.get_status())
}

#[tauri::command]
pub async fn start_discovery(state: State<'_, SharedState>) -> Result<(), String> {
    let s = state.read().await;
    let mut dm = s.device_mgr.write().await;
    dm.set_discovery_running(true);
    dm.discover_devices()
}

#[tauri::command]
pub async fn stop_discovery(state: State<'_, SharedState>) -> Result<(), String> {
    let s = state.read().await;
    let mut dm = s.device_mgr.write().await;
    dm.set_discovery_running(false);
    Ok(())
}
