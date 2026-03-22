use crate::config::{ParamMeta, ParamValue};
use crate::device::protocol::Target;
use crate::state::SharedState;
use serde::{Deserialize, Serialize};
use tauri::State;

#[derive(Deserialize)]
pub struct ConfigGetArgs {
    pub target: String,
    pub param: u16,
}

#[derive(Deserialize)]
pub struct ConfigSetArgs {
    pub target: String,
    pub param: u16,
    pub value: serde_json::Value,
}

#[derive(Serialize)]
pub struct ConfigValueResult {
    pub param: u16,
    pub value: Vec<u8>,
}

fn parse_target(t: &str) -> Result<Target, String> {
    match t {
        "keyboard" | "kb" => Ok(Target::Keyboard),
        "hub" => Ok(Target::Hub),
        _ => Err(format!("Invalid target: {t}")),
    }
}

#[tauri::command]
pub async fn config_get(
    state: State<'_, SharedState>,
    args: ConfigGetArgs,
) -> Result<ConfigValueResult, String> {
    let target = parse_target(&args.target)?;
    let s = state.inner().read().await;
    let mut config = s.config_svc.write().await;
    let mut dm = s.device_mgr.lock().await;
    let value = config.get(target, args.param, &mut dm)?;
    Ok(ConfigValueResult {
        param: args.param,
        value,
    })
}

#[tauri::command]
pub async fn config_set(
    state: State<'_, SharedState>,
    args: ConfigSetArgs,
) -> Result<(), String> {
    let target = parse_target(&args.target)?;
    let s = state.inner().read().await;
    let mut config = s.config_svc.write().await;
    let mut dm = s.device_mgr.lock().await;

    let param_meta = config
        .registry()
        .get(args.param)
        .ok_or("Unknown param")?
        .clone();

    let value = match &param_meta.value_type {
        crate::config::ParamType::U8 | crate::config::ParamType::Enum => {
            ParamValue::U8(args.value.as_u64().ok_or("Expected u8")? as u8)
        }
        crate::config::ParamType::U16 => {
            ParamValue::U16(args.value.as_u64().ok_or("Expected u16")? as u16)
        }
        crate::config::ParamType::Bool => {
            ParamValue::Bool(args.value.as_bool().ok_or("Expected bool")?)
        }
    };

    config.set(target, args.param, &value, &mut dm)
}

#[tauri::command]
pub async fn config_sync_all(state: State<'_, SharedState>) -> Result<(), String> {
    let s = state.inner().read().await;
    let mut config = s.config_svc.write().await;
    let mut dm = s.device_mgr.lock().await;
    config.sync_all(&mut dm)
}

#[tauri::command]
pub async fn get_param_registry(state: State<'_, SharedState>) -> Result<Vec<ParamMeta>, String> {
    let s = state.inner().read().await;
    let config = s.config_svc.read().await;
    Ok(config.registry().get_all().to_vec())
}
