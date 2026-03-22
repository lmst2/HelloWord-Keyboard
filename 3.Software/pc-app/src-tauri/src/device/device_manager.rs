use super::hub_device::HubDevice;
use super::keyboard_device::KeyboardDevice;
use super::protocol::*;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub enum ConnectionState {
    Disconnected,
    Connected,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DeviceStatus {
    pub keyboard: ConnectionState,
    pub hub: ConnectionState,
    pub kb_firmware_version: Option<String>,
    pub hub_firmware_version: Option<String>,
}

pub struct DeviceManager {
    keyboard: Option<KeyboardDevice>,
    hub: Option<HubDevice>,
    discovery_running: bool,
    /// Last (enabled, max_level) sent on CDC to avoid duplicate PC_HUB_LOG_CONFIG spam.
    last_hub_log_config: Option<(bool, u8)>,
}

impl DeviceManager {
    pub fn new() -> Self {
        Self {
            keyboard: None,
            hub: None,
            discovery_running: false,
            last_hub_log_config: None,
        }
    }

    pub fn get_status(&self) -> DeviceStatus {
        DeviceStatus {
            keyboard: if self.keyboard.is_some() {
                ConnectionState::Connected
            } else {
                ConnectionState::Disconnected
            },
            hub: if self.hub.is_some() {
                ConnectionState::Connected
            } else {
                ConnectionState::Disconnected
            },
            kb_firmware_version: None,
            hub_firmware_version: None,
        }
    }

    pub fn discover_devices(&mut self) -> Result<(), String> {
        self.discover_keyboard();
        self.discover_hub();
        Ok(())
    }

    fn discover_keyboard(&mut self) {
        if self.keyboard.is_some() {
            return;
        }
        log::trace!(
            "discover_keyboard: scanning HID VID={:04X} PID={:04X} usage_page={:04X}",
            KB_VID,
            KB_PID,
            KB_RAW_HID_USAGE_PAGE
        );
        let api = match hidapi::HidApi::new() {
            Ok(api) => api,
            Err(e) => {
                log::warn!("Failed to init HID API: {e}");
                return;
            }
        };
        let mut candidates = 0u32;
        for dev_info in api.device_list() {
            if dev_info.vendor_id() == KB_VID
                && dev_info.product_id() == KB_PID
                && dev_info.usage_page() == KB_RAW_HID_USAGE_PAGE
            {
                candidates += 1;
                log::trace!("discover_keyboard: opening candidate path={:?}", dev_info.path());
                match dev_info.open_device(&api) {
                    Ok(device) => {
                        log::info!("Keyboard connected via raw HID: {:?}", dev_info.path());
                        self.keyboard = Some(KeyboardDevice::new(device));
                        return;
                    }
                    Err(e) => log::warn!("Failed to open keyboard HID: {e}"),
                }
            }
        }
        if candidates == 0 {
            log::trace!("discover_keyboard: no matching HID devices");
        }
    }

    fn discover_hub(&mut self) {
        if self.hub.is_some() {
            return;
        }
        log::trace!(
            "discover_hub: listing serial ports for USB VID={:04X} PID={:04X}",
            HUB_VID,
            HUB_PID
        );
        let ports = match serialport::available_ports() {
            Ok(ports) => ports,
            Err(e) => {
                log::warn!("Failed to list serial ports: {e}");
                return;
            }
        };
        log::trace!("discover_hub: {} ports reported", ports.len());
        for port_info in ports {
            if let serialport::SerialPortType::UsbPort(usb) = &port_info.port_type {
                log::trace!(
                    "serial port {} usb VID={:04X} PID={:04X}",
                    port_info.port_name,
                    usb.vid,
                    usb.pid
                );
                if usb.vid == HUB_VID && usb.pid == HUB_PID {
                    match serialport::new(&port_info.port_name, 115200)
                        .timeout(std::time::Duration::from_millis(100))
                        .open()
                    {
                        Ok(port) => {
                            log::info!("Hub connected via CDC: {}", port_info.port_name);
                            self.hub = Some(HubDevice::new(port));
                            self.last_hub_log_config = None;
                            return;
                        }
                        Err(e) => log::warn!("Failed to open hub serial: {e}"),
                    }
                }
            }
        }
        log::trace!("discover_hub: no hub CDC port opened");
    }

    pub fn is_keyboard_connected(&self) -> bool {
        self.keyboard.is_some()
    }
    pub fn is_hub_connected(&self) -> bool {
        self.hub.is_some()
    }
    pub fn set_discovery_running(&mut self, running: bool) {
        self.discovery_running = running;
    }
    pub fn is_discovery_running(&self) -> bool {
        self.discovery_running
    }

    // ---- Keyboard HID operations ----
    pub fn kb_send(&self, cmd: u8, payload: &[u8]) -> Result<(), String> {
        self.keyboard.as_ref().ok_or("Keyboard not connected")?.send(cmd, payload)
    }
    pub fn kb_config_get(&self, param: u16) -> Result<Vec<u8>, String> {
        self.keyboard.as_ref().ok_or("Keyboard not connected")?.config_get(param)
    }
    pub fn kb_config_set(&self, param: u16, value: &[u8]) -> Result<(), String> {
        self.keyboard.as_ref().ok_or("Keyboard not connected")?.config_set(param, value)
    }
    pub fn kb_config_get_all(&self) -> Result<Vec<(u16, Vec<u8>)>, String> {
        self.keyboard.as_ref().ok_or("Keyboard not connected")?.config_get_all()
    }
    pub fn kb_rgb_direct(&self, page: u8, colors: &[u8]) -> Result<(), String> {
        self.keyboard.as_ref().ok_or("Keyboard not connected")?.rgb_direct(page, colors)
    }
    pub fn kb_rgb_stop(&self) -> Result<(), String> {
        self.keyboard.as_ref().ok_or("Keyboard not connected")?.rgb_stop()
    }
    pub fn kb_rgb_set_effect(&self, effect_id: u8) -> Result<(), String> {
        self.keyboard.as_ref().ok_or("Keyboard not connected")?.rgb_set_effect(effect_id)
    }
    pub fn kb_status_request(&self) -> Result<Message, String> {
        self.keyboard.as_ref().ok_or("Keyboard not connected")?.status_request()
    }
    pub fn kb_enter_dfu(&self) -> Result<(), String> {
        self.keyboard.as_ref().ok_or("Keyboard not connected")?.enter_dfu()
    }

    // ---- Hub CDC operations ----
    pub fn hub_config_get(&mut self, target: Target, param: u16) -> Result<Vec<u8>, String> {
        self.hub.as_mut().ok_or("Hub not connected")?.config_get(target, param)
    }
    pub fn hub_config_set(&mut self, target: Target, param: u16, value: &[u8]) -> Result<(), String> {
        self.hub.as_mut().ok_or("Hub not connected")?.config_set(target, param, value)
    }
    pub fn hub_data_feed(&mut self, feed_id: u8, data: &[u8]) -> Result<(), String> {
        self.hub.as_mut().ok_or("Hub not connected")?.data_feed(feed_id, data)
    }
    pub fn hub_eink_upload(&mut self, slot: u8, page: u8, data: &[u8]) -> Result<(), String> {
        self.hub.as_mut().ok_or("Hub not connected")?.eink_upload(slot, page, data)
    }
    pub fn hub_eink_send_text(&mut self, text: &str) -> Result<(), String> {
        self.hub.as_mut().ok_or("Hub not connected")?.eink_send_text(text)
    }
    pub fn hub_request_fw_info(&mut self) -> Result<Message, String> {
        self.hub.as_mut().ok_or("Hub not connected")?.request_fw_info()
    }
    pub fn hub_profile_list(&mut self) -> Result<Message, String> {
        self.hub.as_mut().ok_or("Hub not connected")?.profile_list()
    }
    pub fn hub_profile_save(&mut self, slot: u8, name: &str) -> Result<(), String> {
        self.hub.as_mut().ok_or("Hub not connected")?.profile_save(slot, name)
    }
    pub fn hub_profile_load(&mut self, slot: u8) -> Result<(), String> {
        self.hub.as_mut().ok_or("Hub not connected")?.profile_load(slot)
    }
    pub fn hub_profile_delete(&mut self, slot: u8) -> Result<(), String> {
        self.hub.as_mut().ok_or("Hub not connected")?.profile_delete(slot)
    }
    pub fn hub_switch_primary_app(&mut self, app_id: u8) -> Result<(), String> {
        self.hub.as_mut().ok_or("Hub not connected")?.switch_primary_app(app_id)
    }
    pub fn hub_switch_eink_app(&mut self, app_id: u8) -> Result<(), String> {
        self.hub.as_mut().ok_or("Hub not connected")?.switch_eink_app(app_id)
    }
    pub fn hub_enter_dfu_kb(&mut self) -> Result<(), String> {
        self.hub.as_mut().ok_or("Hub not connected")?.enter_dfu_kb()
    }
    pub fn hub_enter_dfu_hub(&mut self) -> Result<(), String> {
        self.hub.as_mut().ok_or("Hub not connected")?.enter_dfu_hub()
    }

    pub fn disconnect_keyboard(&mut self) {
        self.keyboard = None;
    }
    pub fn disconnect_hub(&mut self) {
        self.hub = None;
        self.last_hub_log_config = None;
    }

    /// Drain CDC backlog (device log lines + push other cmds to hub pending queue).
    pub fn hub_drain_logs(
        &mut self,
        out: &mut Vec<crate::device_log::DeviceLogLine>,
    ) -> Result<(), String> {
        match self.hub.as_mut() {
            Some(h) => h.drain_unsolicited(out),
            None => Ok(()),
        }
    }

    pub fn hub_log_config(&mut self, enabled: bool, max_level: u8) -> Result<(), String> {
        let max_level = max_level.min(3);
        if self.last_hub_log_config == Some((enabled, max_level)) {
            log::trace!(
                "hub_log_config: unchanged enabled={} max_level={}, skip CDC send",
                enabled,
                max_level
            );
            return Ok(());
        }
        self.hub
            .as_mut()
            .ok_or_else(|| "Hub not connected".to_string())?
            .log_config(enabled, max_level)?;
        self.last_hub_log_config = Some((enabled, max_level));
        log::debug!(
            "hub_log_config: applied enabled={} max_level={}",
            enabled,
            max_level
        );
        Ok(())
    }
}
