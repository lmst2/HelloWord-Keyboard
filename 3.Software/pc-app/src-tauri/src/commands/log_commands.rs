use crate::device_log::DeviceLogLine;
use crate::state::SharedState;
use tauri::State;

#[tauri::command]
pub async fn log_get_snapshot(state: State<'_, SharedState>) -> Result<Vec<DeviceLogLine>, String> {
    let s = state.inner().read().await;
    let store = s
        .device_log_store
        .lock()
        .map_err(|e| format!("log store lock: {e}"))?;
    Ok(store.snapshot())
}

#[tauri::command]
pub async fn log_clear(state: State<'_, SharedState>) -> Result<(), String> {
    let s = state.inner().read().await;
    let mut store = s
        .device_log_store
        .lock()
        .map_err(|e| format!("log store lock: {e}"))?;
    store.clear();
    Ok(())
}

/// Push current settings to hub (device log enable + max level).
#[tauri::command]
pub async fn log_sync_device(state: State<'_, SharedState>) -> Result<(), String> {
    let s = state.inner().read().await;
    let settings = s.settings.read().await.clone();
    let mut dm = s.device_mgr.lock().await;
    if dm.is_hub_connected() {
        dm.hub_log_config(settings.device_log_enabled, settings.device_log_max_level)
    } else {
        Ok(())
    }
}
