use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AppSettings {
    pub minimize_to_tray: bool,
    pub auto_start: bool,
    pub theme: String,
    pub data_providers_enabled: Vec<String>,
    pub auto_detect_os: bool,
    pub hid_vid: u16,
    pub hid_pid: u16,
    pub cdc_vid: u16,
    pub cdc_pid: u16,
    pub openrgb_enabled: bool,
    pub openrgb_host: String,
    pub openrgb_port: u16,
    pub weather_city: String,
    pub weather_api_key: String,
}

impl Default for AppSettings {
    fn default() -> Self {
        Self {
            minimize_to_tray: true,
            auto_start: false,
            theme: "dark".to_string(),
            data_providers_enabled: vec!["system_stats".to_string(), "clock".to_string()],
            auto_detect_os: true,
            hid_vid: 0x1001,
            hid_pid: 0xF103,
            cdc_vid: 0x1001,
            cdc_pid: 0x03EF,
            openrgb_enabled: false,
            openrgb_host: "127.0.0.1".to_string(),
            openrgb_port: 6742,
            weather_city: "".to_string(),
            weather_api_key: "".to_string(),
        }
    }
}

impl AppSettings {
    pub fn load_or_default() -> Self {
        Self::config_path()
            .and_then(|path| std::fs::read_to_string(path).ok())
            .and_then(|json| serde_json::from_str(&json).ok())
            .unwrap_or_default()
    }

    pub fn save(&self) -> Result<(), String> {
        let path = Self::config_path().ok_or("Cannot determine config directory")?;
        if let Some(parent) = path.parent() {
            std::fs::create_dir_all(parent).map_err(|e| format!("Create config dir: {e}"))?;
        }
        let json =
            serde_json::to_string_pretty(self).map_err(|e| format!("Serialize: {e}"))?;
        std::fs::write(path, json).map_err(|e| format!("Write: {e}"))?;

        // Platform auto-start registration
        self.apply_auto_start();
        Ok(())
    }

    fn config_path() -> Option<std::path::PathBuf> {
        dirs::config_dir().map(|d| d.join("helloword-manager").join("settings.json"))
    }

    fn apply_auto_start(&self) {
        #[cfg(target_os = "windows")]
        {
            let exe = std::env::current_exe().unwrap_or_default();
            let hkcu = winreg_open();
            if let Some(key) = hkcu {
                if self.auto_start {
                    let exe_path = exe.to_string_lossy().into_owned();
                    let _ = key.set_value("HelloWord-Manager", &exe_path);
                } else {
                    let _ = key.delete_value("HelloWord-Manager");
                }
            }
        }
        #[cfg(not(target_os = "windows"))]
        {
            // macOS/Linux: create/remove .desktop or LaunchAgent file
            let _ = self.auto_start; // suppress unused warning
        }
    }
}

#[cfg(target_os = "windows")]
fn winreg_open() -> Option<winreg::RegKey> {
    use winreg::enums::{HKEY_CURRENT_USER, KEY_WRITE};

    let hkcu = winreg::RegKey::predef(HKEY_CURRENT_USER);
    hkcu.open_subkey_with_flags(
        r"Software\Microsoft\Windows\CurrentVersion\Run",
        KEY_WRITE,
    )
    .ok()
}
