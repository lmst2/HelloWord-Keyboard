use super::protocol::*;
use hidapi::HidDevice;

/// Keyboard HID bootloader flash protocol.
/// Protocol: 64-byte interrupt IN/OUT reports.
/// Commands: INFO, ERASE, WRITE (60B chunks), SEAL (CRC32), REBOOT.
pub struct BootloaderDevice {
    device: HidDevice,
}

#[derive(Debug, Clone)]
pub struct BootloaderInfo {
    pub app_size: u32,
    pub page_size: u16,
    pub version_major: u8,
    pub version_minor: u8,
}

impl BootloaderDevice {
    pub fn new(device: HidDevice) -> Self {
        Self { device }
    }

    fn send_recv(&self, payload: &[u8], timeout_ms: i32) -> Result<Vec<u8>, String> {
        let mut buf = vec![0u8; BL_REPORT_SIZE];
        // buf[0] = 0x00 (report ID for bootloader)
        let copy_len = payload.len().min(BL_REPORT_SIZE - 1);
        buf[1..1 + copy_len].copy_from_slice(&payload[..copy_len]);
        self.device
            .write(&buf)
            .map_err(|e| format!("BL write error: {e}"))?;

        let mut resp = vec![0u8; 64];
        let n = self
            .device
            .read_timeout(&mut resp, timeout_ms)
            .map_err(|e| format!("BL read error: {e}"))?;
        if n < 2 {
            return Err("No response from bootloader".into());
        }
        Ok(resp[..n].to_vec())
    }

    fn check_response(resp: &[u8], expected_cmd: u8) -> Result<(), String> {
        if resp[0] != expected_cmd {
            return Err(format!(
                "Unexpected response 0x{:02X} for cmd 0x{expected_cmd:02X}",
                resp[0]
            ));
        }
        if resp[1] != BL_RSP_OK {
            let status = match resp[1] {
                BL_RSP_ERR => "error",
                BL_RSP_BAD_STATE => "bad state (erase first?)",
                BL_RSP_BAD_ADDR => "bad address",
                BL_RSP_CRC_FAIL => "CRC mismatch",
                _ => "unknown",
            };
            return Err(format!("Bootloader error: {status}"));
        }
        Ok(())
    }

    pub fn get_info(&self) -> Result<BootloaderInfo, String> {
        let resp = self.send_recv(&[BL_CMD_INFO], 5000)?;
        if resp[0] != BL_CMD_INFO || resp.len() < 9 {
            return Err("Bad info response".into());
        }
        Ok(BootloaderInfo {
            app_size: u32::from_le_bytes([resp[1], resp[2], resp[3], resp[4]]),
            page_size: u16::from_le_bytes([resp[5], resp[6]]),
            version_major: resp[7],
            version_minor: resp[8],
        })
    }

    pub fn erase(&self) -> Result<(), String> {
        let resp = self.send_recv(&[BL_CMD_ERASE], 30000)?;
        Self::check_response(&resp, BL_CMD_ERASE)
    }

    pub fn write_chunk(&self, offset: u32, data: &[u8]) -> Result<(), String> {
        let mut pkt = vec![
            BL_CMD_WRITE,
            (offset & 0xFF) as u8,
            ((offset >> 8) & 0xFF) as u8,
            ((offset >> 16) & 0xFF) as u8,
        ];
        pkt.extend_from_slice(data);
        let resp = self.send_recv(&pkt, 5000)?;
        Self::check_response(&resp, BL_CMD_WRITE)
    }

    pub fn seal(&self, size: u32, crc: u32) -> Result<(), String> {
        let mut pkt = vec![BL_CMD_SEAL];
        pkt.extend_from_slice(&size.to_le_bytes());
        pkt.extend_from_slice(&crc.to_le_bytes());
        let resp = self.send_recv(&pkt, 10000)?;
        Self::check_response(&resp, BL_CMD_SEAL)
    }

    pub fn reboot(&self) -> Result<(), String> {
        let _ = self.send_recv(&[BL_CMD_REBOOT], 2000);
        Ok(())
    }

    /// Full flash sequence: erase → write → verify CRC → reboot.
    /// Returns progress (0.0..1.0) via callback.
    pub fn flash_firmware(
        &self,
        firmware: &[u8],
        on_progress: impl Fn(f32, &str),
    ) -> Result<(), String> {
        let info = self.get_info()?;
        on_progress(0.0, &format!(
            "Bootloader v{}.{}, app region {}KB",
            info.version_major, info.version_minor, info.app_size / 1024
        ));

        if firmware.len() as u32 > info.app_size {
            return Err(format!(
                "Firmware too large ({} > {} bytes)",
                firmware.len(),
                info.app_size
            ));
        }

        on_progress(0.02, "Erasing flash...");
        self.erase()?;

        let total = firmware.len();
        let mut offset = 0usize;
        while offset < total {
            let end = (offset + BL_WRITE_CHUNK).min(total);
            let mut chunk = firmware[offset..end].to_vec();
            // Pad last chunk with 0xFF
            while chunk.len() < BL_WRITE_CHUNK {
                chunk.push(0xFF);
            }
            self.write_chunk(offset as u32, &chunk)?;
            offset += BL_WRITE_CHUNK;
            let pct = 0.05 + 0.85 * (offset.min(total) as f32 / total as f32);
            on_progress(pct, &format!("Writing {}/{}...", offset.min(total), total));
        }

        on_progress(0.92, "Verifying CRC32...");
        let crc = crc32_compute(firmware);
        self.seal(total as u32, crc)?;

        on_progress(0.98, "Rebooting to application...");
        self.reboot()?;
        on_progress(1.0, "Flash complete!");
        Ok(())
    }
}

/// Open bootloader HID device if present
pub fn open_bootloader() -> Option<BootloaderDevice> {
    let api = hidapi::HidApi::new().ok()?;
    for dev_info in api.device_list() {
        if dev_info.vendor_id() == BL_VID && dev_info.product_id() == BL_PID {
            if let Ok(device) = dev_info.open_device(&api) {
                return Some(BootloaderDevice::new(device));
            }
        }
    }
    None
}

/// Wait for bootloader to enumerate after reset
pub fn wait_for_bootloader(timeout: std::time::Duration) -> Option<BootloaderDevice> {
    let deadline = std::time::Instant::now() + timeout;
    while std::time::Instant::now() < deadline {
        if let Some(bl) = open_bootloader() {
            return Some(bl);
        }
        std::thread::sleep(std::time::Duration::from_millis(300));
    }
    None
}

fn crc32_compute(data: &[u8]) -> u32 {
    let mut crc: u32 = 0xFFFF_FFFF;
    for &byte in data {
        crc ^= byte as u32;
        for _ in 0..8 {
            if crc & 1 != 0 {
                crc = (crc >> 1) ^ 0xEDB8_8320;
            } else {
                crc >>= 1;
            }
        }
    }
    crc ^ 0xFFFF_FFFF
}
