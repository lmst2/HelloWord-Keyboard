use super::param_registry::{ParamRegistry, ParamValue};
use crate::device::protocol::Target;
use crate::device::DeviceManager;
use std::collections::HashMap;

pub struct ConfigService {
    kb_cache: HashMap<u16, Vec<u8>>,
    hub_cache: HashMap<u16, Vec<u8>>,
    registry: ParamRegistry,
}

impl ConfigService {
    pub fn new() -> Self {
        Self {
            kb_cache: HashMap::new(),
            hub_cache: HashMap::new(),
            registry: ParamRegistry::new(),
        }
    }

    pub fn registry(&self) -> &ParamRegistry {
        &self.registry
    }

    pub fn sync_all(&mut self, device_mgr: &mut DeviceManager) -> Result<(), String> {
        if device_mgr.is_keyboard_connected() {
            match device_mgr.kb_config_get_all() {
                Ok(params) => {
                    self.kb_cache.clear();
                    for (id, val) in params {
                        self.kb_cache.insert(id, val);
                    }
                    log::info!("Synced {} keyboard params", self.kb_cache.len());
                }
                Err(e) => log::warn!("Failed to sync keyboard config: {e}"),
            }
        }
        Ok(())
    }

    pub fn get_cached(&self, target: Target, param: u16) -> Option<&Vec<u8>> {
        match target {
            Target::Keyboard => self.kb_cache.get(&param),
            Target::Hub => self.hub_cache.get(&param),
        }
    }

    pub fn get(
        &mut self,
        target: Target,
        param: u16,
        device_mgr: &mut DeviceManager,
    ) -> Result<Vec<u8>, String> {
        match target {
            Target::Keyboard => {
                let val = device_mgr.kb_config_get(param)?;
                self.kb_cache.insert(param, val.clone());
                Ok(val)
            }
            Target::Hub => {
                let val = device_mgr.hub_config_get(target, param)?;
                self.hub_cache.insert(param, val.clone());
                Ok(val)
            }
        }
    }

    pub fn set(
        &mut self,
        target: Target,
        param: u16,
        value: &ParamValue,
        device_mgr: &mut DeviceManager,
    ) -> Result<(), String> {
        let bytes = value.to_bytes();
        match target {
            Target::Keyboard => {
                if device_mgr.is_keyboard_connected() {
                    device_mgr.kb_config_set(param, &bytes)?;
                } else if device_mgr.is_hub_connected() {
                    device_mgr.hub_config_set(target, param, &bytes)?;
                } else {
                    return Err("No device connected".into());
                }
                self.kb_cache.insert(param, bytes);
            }
            Target::Hub => {
                device_mgr.hub_config_set(target, param, &bytes)?;
                self.hub_cache.insert(param, bytes);
            }
        }
        Ok(())
    }

    pub fn get_all_cached(&self, target: Target) -> &HashMap<u16, Vec<u8>> {
        match target {
            Target::Keyboard => &self.kb_cache,
            Target::Hub => &self.hub_cache,
        }
    }
}
