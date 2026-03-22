use crate::device::bootloader_device::{open_bootloader, wait_for_bootloader};
use crate::device::DeviceManager;
use serde::{Deserialize, Serialize};
use std::sync::{Arc, Mutex};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FirmwareInfo {
    pub kb_version: Option<String>,
    pub hub_version: Option<String>,
    pub build_time: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FlashProgress {
    pub progress: f32,
    pub message: String,
    pub done: bool,
    pub error: Option<String>,
}

pub struct DfuService {
    pub progress: Arc<Mutex<FlashProgress>>,
}

impl DfuService {
    pub fn new() -> Self {
        Self {
            progress: Arc::new(Mutex::new(FlashProgress {
                progress: 0.0,
                message: "Idle".into(),
                done: false,
                error: None,
            })),
        }
    }

    pub fn parse_fw_info(payload: &[u8]) -> FirmwareInfo {
        let kb_ver = if payload.len() >= 2 {
            Some(format!("{}.{}", payload[0], payload[1]))
        } else {
            None
        };
        let hub_ver = if payload.len() >= 4 {
            Some(format!("{}.{}", payload[2], payload[3]))
        } else {
            None
        };
        let build_time = if payload.len() > 4 {
            String::from_utf8(payload[4..].to_vec()).ok()
        } else {
            None
        };
        FirmwareInfo {
            kb_version: kb_ver,
            hub_version: hub_ver,
            build_time,
        }
    }

    /// Full automated keyboard flash:
    /// 1. Send DFU command to keyboard (via HID or Hub relay)
    /// 2. Wait for bootloader to enumerate
    /// 3. Flash firmware using bootloader HID protocol
    /// 4. Bootloader auto-reboots to new firmware
    pub fn flash_keyboard(
        &self,
        device_mgr: &mut DeviceManager,
        firmware: &[u8],
    ) -> Result<(), String> {
        let progress = self.progress.clone();
        Self::update_progress(&progress, 0.0, "Starting keyboard flash...");

        // Step 1: Check if bootloader is already present
        let bl = if let Some(bl) = open_bootloader() {
            Self::update_progress(&progress, 0.05, "Bootloader already active");
            bl
        } else {
            // Send DFU reset command
            Self::update_progress(&progress, 0.02, "Sending DFU reset to keyboard...");

            if device_mgr.is_keyboard_connected() {
                device_mgr.kb_enter_dfu()?;
            } else if device_mgr.is_hub_connected() {
                device_mgr.hub_enter_dfu_kb()?;
            } else {
                return Err("No device connected to trigger DFU".into());
            }

            // Keyboard disconnects from normal HID, give it time
            device_mgr.disconnect_keyboard();
            Self::update_progress(&progress, 0.04, "Waiting for bootloader...");

            wait_for_bootloader(std::time::Duration::from_secs(8))
                .ok_or("Bootloader did not appear after reset (timeout 8s)")?
        };

        // Step 2: Flash using bootloader protocol
        let progress_cb = {
            let p = progress.clone();
            move |pct: f32, msg: &str| {
                let scaled = 0.05 + pct * 0.95;
                Self::update_progress(&p, scaled, msg);
            }
        };

        bl.flash_firmware(firmware, progress_cb)?;

        Self::update_progress(&progress, 1.0, "Keyboard firmware updated successfully!");
        {
            let mut p = progress.lock().unwrap();
            p.done = true;
        }
        Ok(())
    }

    /// Hub DFU: send reset command, Hub enters STM32 system bootloader (USB DFU mode).
    /// Actual flashing requires dfu-util (STM32 DFU protocol) — we trigger the reset
    /// and provide instructions.
    pub fn flash_hub(
        &self,
        device_mgr: &mut DeviceManager,
        _firmware: &[u8],
    ) -> Result<(), String> {
        let progress = self.progress.clone();
        Self::update_progress(&progress, 0.0, "Triggering Hub DFU mode...");

        device_mgr.hub_enter_dfu_hub()?;
        device_mgr.disconnect_hub();

        Self::update_progress(&progress, 0.5,
            "Hub entered DFU mode. STM32 system bootloader active on USB.");

        // For Hub (STM32F405), the system bootloader uses USB DFU class.
        // We could shell out to dfu-util, or implement DFU protocol.
        // For now, provide the command the user can run:
        Self::update_progress(&progress, 1.0,
            "Hub in DFU mode. Use dfu-util to flash: dfu-util -a 0 -s 0x08000000 -D firmware.bin");

        {
            let mut p = progress.lock().unwrap();
            p.done = true;
        }
        Ok(())
    }

    pub fn get_progress(&self) -> FlashProgress {
        self.progress.lock().unwrap().clone()
    }

    pub fn reset_progress(&self) {
        let mut p = self.progress.lock().unwrap();
        *p = FlashProgress {
            progress: 0.0,
            message: "Idle".into(),
            done: false,
            error: None,
        };
    }

    fn update_progress(progress: &Arc<Mutex<FlashProgress>>, pct: f32, msg: &str) {
        if let Ok(mut p) = progress.lock() {
            p.progress = pct;
            p.message = msg.to_string();
            log::info!("[DFU {:.0}%] {}", pct * 100.0, msg);
        }
    }
}
