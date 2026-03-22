use crate::rgb::RgbMode;
use crate::state::SharedState;
use serde::Serialize;
use tauri::State;

#[derive(Serialize)]
pub struct RgbModeInfo {
    pub mode: String,
    pub running: bool,
}

#[tauri::command]
pub async fn rgb_set_mode(
    state: State<'_, SharedState>,
    mode: serde_json::Value,
) -> Result<(), String> {
    let s = state.inner().read().await;
    let mut engine = s.rgb_engine.write().await;

    let rgb_mode: RgbMode =
        serde_json::from_value(mode).map_err(|e| format!("Invalid RGB mode: {e}"))?;
    engine.set_mode(rgb_mode);
    engine.start();
    Ok(())
}

#[tauri::command]
pub async fn rgb_get_mode(state: State<'_, SharedState>) -> Result<RgbModeInfo, String> {
    let s = state.inner().read().await;
    let engine = s.rgb_engine.read().await;
    let mode_name = match engine.get_mode() {
        RgbMode::Off => "off".to_string(),
        RgbMode::CpuTempGradient { .. } => "cpu_temp".to_string(),
        RgbMode::StaticColor(_) => "static".to_string(),
    };
    Ok(RgbModeInfo {
        mode: mode_name,
        running: engine.is_running(),
    })
}

#[tauri::command]
pub async fn rgb_stop(state: State<'_, SharedState>) -> Result<(), String> {
    let s = state.inner().read().await;
    let mut engine = s.rgb_engine.write().await;
    engine.stop();
    engine.set_mode(RgbMode::Off);
    Ok(())
}
