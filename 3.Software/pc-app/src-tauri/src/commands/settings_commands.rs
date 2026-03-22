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
    let s = state.inner().read().await;
    let mut settings = s.settings.write().await;
    *settings = new_settings;
    settings.save()
}
