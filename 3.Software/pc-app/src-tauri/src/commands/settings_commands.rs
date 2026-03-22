use crate::logging;
use crate::settings::AppSettings;
use crate::state::SharedState;
use tauri::State;

#[tauri::command]
pub async fn settings_get(state: State<'_, SharedState>) -> Result<AppSettings, String> {
    let s = state.inner().read().await;
    let settings = s.settings.read().await;
    Ok(settings.clone())
}

#[tauri::command]
pub async fn settings_set(
    state: State<'_, SharedState>,
    new_settings: AppSettings,
) -> Result<(), String> {
    {
        let s = state.inner().read().await;
        let mut settings = s.settings.write().await;
        *settings = new_settings.clone();
        settings.save()?;
    }

    logging::apply_pc_rust_log_level(&new_settings.pc_app_log_level);

    {
        let s = state.inner().read().await;
        let mut dm = s.device_mgr.lock().await;
        let _ = dm.hub_log_config(
            new_settings.device_log_enabled,
            new_settings.device_log_max_level,
        );
    }

    Ok(())
}
