use crate::config::ConfigService;
use crate::device::protocol::Target;
use crate::device::DeviceManager;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::path::Path;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProfileInfo {
    pub slot: u8,
    pub name: String,
    pub used: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProfileData {
    pub name: String,
    pub kb_params: HashMap<String, Vec<u8>>,
    pub hub_params: HashMap<String, Vec<u8>>,
}

pub struct ProfileService;

impl ProfileService {
    pub fn new() -> Self {
        Self
    }

    /// Export current device config to JSON file
    pub fn export_to_file(
        &self,
        name: &str,
        config_svc: &ConfigService,
        path: &Path,
    ) -> Result<(), String> {
        let mut kb_params = HashMap::new();
        for (id, val) in config_svc.get_all_cached(Target::Keyboard) {
            kb_params.insert(format!("0x{id:04X}"), val.clone());
        }
        let mut hub_params = HashMap::new();
        for (id, val) in config_svc.get_all_cached(Target::Hub) {
            hub_params.insert(format!("0x{id:04X}"), val.clone());
        }

        let profile = ProfileData {
            name: name.to_string(),
            kb_params,
            hub_params,
        };

        let json =
            serde_json::to_string_pretty(&profile).map_err(|e| format!("Serialize error: {e}"))?;
        std::fs::write(path, json).map_err(|e| format!("Write error: {e}"))
    }

    /// Import profile from JSON and push all params to device
    pub fn import_from_file(
        &self,
        path: &Path,
        config_svc: &mut ConfigService,
        device_mgr: &mut DeviceManager,
    ) -> Result<String, String> {
        let json = std::fs::read_to_string(path).map_err(|e| format!("Read error: {e}"))?;
        let profile: ProfileData =
            serde_json::from_str(&json).map_err(|e| format!("Parse error: {e}"))?;

        for (id_str, value) in &profile.kb_params {
            let id = u16::from_str_radix(id_str.trim_start_matches("0x"), 16)
                .map_err(|e| format!("Bad param ID {id_str}: {e}"))?;
            let pv = crate::config::ParamValue::U8(value[0]);
            let _ = config_svc.set(Target::Keyboard, id, &pv, device_mgr);
        }
        for (id_str, value) in &profile.hub_params {
            let id = u16::from_str_radix(id_str.trim_start_matches("0x"), 16)
                .map_err(|e| format!("Bad param ID {id_str}: {e}"))?;
            let pv = crate::config::ParamValue::U8(value[0]);
            let _ = config_svc.set(Target::Hub, id, &pv, device_mgr);
        }

        Ok(profile.name)
    }
}
