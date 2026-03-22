use crate::device::DeviceStatus;
use crate::state::SharedState;
use tauri::State;

#[tauri::command]
pub async fn get_device_status(state: State<'_, SharedState>) -> Result<DeviceStatus, String> {
    let s = state.inner().read().await;
    let dm = s.device_mgr.lock().await;
    Ok(dm.get_status())
}

#[tauri::command]
pub async fn start_discovery(state: State<'_, SharedState>) -> Result<(), String> {
    let s = state.inner().read().await;
    let settings = s.settings.read().await.clone();
    let mut dm = s.device_mgr.lock().await;
    dm.set_discovery_running(true);
    dm.discover_devices()?;
    let _ = dm.hub_log_config(settings.device_log_enabled, settings.device_log_max_level);
    Ok(())
}

#[tauri::command]
pub async fn stop_discovery(state: State<'_, SharedState>) -> Result<(), String> {
    let s = state.inner().read().await;
    let mut dm = s.device_mgr.lock().await;
    dm.set_discovery_running(false);
    Ok(())
}
