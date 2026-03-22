use crate::data::{FeedData, ProviderInfo};
use crate::state::SharedState;
use tauri::State;

#[tauri::command]
pub async fn data_start(state: State<'_, SharedState>) -> Result<(), String> {
    log::info!("data_start: PC data feed engine running (tray will push to Hub)");
    let s = state.inner().read().await;
    let mut engine = s.data_engine.write().await;
    engine.start();
    Ok(())
}

#[tauri::command]
pub async fn data_stop(state: State<'_, SharedState>) -> Result<(), String> {
    log::info!("data_stop: PC data feed engine stopped");
    let s = state.inner().read().await;
    let mut engine = s.data_engine.write().await;
    engine.stop();
    Ok(())
}

#[tauri::command]
pub async fn data_get_providers(
    state: State<'_, SharedState>,
) -> Result<Vec<ProviderInfo>, String> {
    let s = state.inner().read().await;
    let engine = s.data_engine.read().await;
    Ok(engine.get_providers())
}

#[tauri::command]
pub async fn data_set_provider_enabled(
    state: State<'_, SharedState>,
    id: String,
    enabled: bool,
) -> Result<(), String> {
    log::info!("data_set_provider_enabled: id={:?} enabled={}", id, enabled);
    let s = state.inner().read().await;
    let mut engine = s.data_engine.write().await;
    engine.set_provider_enabled(&id, enabled);
    Ok(())
}

#[tauri::command]
pub async fn data_get_live(state: State<'_, SharedState>) -> Result<Vec<FeedData>, String> {
    let s = state.inner().read().await;
    let mut engine = s.data_engine.write().await;
    Ok(engine.poll())
}
