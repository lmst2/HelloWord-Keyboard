use crate::state::SharedState;
use serde::Serialize;
use tauri::State;

#[derive(Serialize)]
pub struct ProfileEntry {
    pub slot: u8,
    pub name: String,
    pub used: bool,
}

#[tauri::command]
pub async fn profile_list(state: State<'_, SharedState>) -> Result<Vec<ProfileEntry>, String> {
    let s = state.inner().read().await;
    let mut dm = s.device_mgr.lock().await;
    let msg = dm.hub_profile_list()?;

    let mut profiles = Vec::new();
    if msg.payload.is_empty() {
        return Ok(profiles);
    }
    let count = msg.payload[0] as usize;
    let mut offset = 1;
    for _ in 0..count {
        if offset >= msg.payload.len() {
            break;
        }
        let slot = msg.payload[offset];
        offset += 1;
        let name_end = msg.payload[offset..]
            .iter()
            .position(|&b| b == 0)
            .unwrap_or(msg.payload.len() - offset);
        let name = String::from_utf8_lossy(&msg.payload[offset..offset + name_end]).to_string();
        offset += name_end + 1;
        profiles.push(ProfileEntry {
            slot,
            name,
            used: true,
        });
    }
    Ok(profiles)
}

#[tauri::command]
pub async fn profile_save(
    state: State<'_, SharedState>,
    slot: u8,
    name: String,
) -> Result<(), String> {
    let s = state.inner().read().await;
    let mut dm = s.device_mgr.lock().await;
    dm.hub_profile_save(slot, &name)
}

#[tauri::command]
pub async fn profile_load(state: State<'_, SharedState>, slot: u8) -> Result<(), String> {
    let s = state.inner().read().await;
    let mut dm = s.device_mgr.lock().await;
    dm.hub_profile_load(slot)
}

#[tauri::command]
pub async fn profile_delete(state: State<'_, SharedState>, slot: u8) -> Result<(), String> {
    let s = state.inner().read().await;
    let mut dm = s.device_mgr.lock().await;
    dm.hub_profile_delete(slot)
}

#[tauri::command]
pub async fn profile_export(
    state: State<'_, SharedState>,
    name: String,
    path: String,
) -> Result<(), String> {
    let s = state.inner().read().await;
    let config = s.config_svc.read().await;
    s.profile_svc
        .export_to_file(&name, &config, std::path::Path::new(&path))
}

#[tauri::command]
pub async fn profile_import(
    state: State<'_, SharedState>,
    path: String,
) -> Result<String, String> {
    let s = state.inner().read().await;
    let mut config = s.config_svc.write().await;
    let mut dm = s.device_mgr.lock().await;
    s.profile_svc
        .import_from_file(std::path::Path::new(&path), &mut config, &mut dm)
}
