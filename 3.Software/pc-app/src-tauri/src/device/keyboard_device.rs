use super::protocol::*;
use hidapi::HidDevice;
use std::time::Duration;

pub struct KeyboardDevice {
    device: HidDevice,
}

impl KeyboardDevice {
    pub fn new(device: HidDevice) -> Self {
        device.set_blocking_mode(false).ok();
        Self { device }
    }

    pub fn send(&self, cmd: u8, payload: &[u8]) -> Result<(), String> {
        let msg = Message::new(cmd, payload.to_vec());
        let report = msg.to_hid_report();
        self.device
            .write(&report)
            .map_err(|e| format!("HID write error: {e}"))?;
        Ok(())
    }

    pub fn recv(&self, timeout: Duration) -> Result<Option<Message>, String> {
        let mut buf = [0u8; 64];
        let n = self
            .device
            .read_timeout(&mut buf, timeout.as_millis() as i32)
            .map_err(|e| format!("HID read error: {e}"))?;
        if n == 0 {
            return Ok(None);
        }
        // hidapi strips report ID on read; buf[0] = CMD
        Ok(Message::from_hid_report(&buf[..n]))
    }

    pub fn config_get(&self, param: u16) -> Result<Vec<u8>, String> {
        let payload = vec![(param >> 8) as u8, (param & 0xFF) as u8];
        self.send(PC_KB_CONFIG_GET, &payload)?;
        let resp = self.recv_expect(KB_PC_CONFIG_VALUE, Duration::from_millis(500))?;
        // resp.payload = [paramHi, paramLo, value...]
        if resp.payload.len() >= 2 {
            Ok(resp.payload[2..].to_vec())
        } else {
            Err("Invalid config value response".into())
        }
    }

    pub fn config_set(&self, param: u16, value: &[u8]) -> Result<(), String> {
        let mut payload = vec![(param >> 8) as u8, (param & 0xFF) as u8];
        payload.extend_from_slice(value);
        self.send(PC_KB_CONFIG_SET, &payload)?;
        let resp = self.recv_expect(KB_PC_ACK, Duration::from_millis(500))?;
        // resp.payload = [original_cmd, result_code]
        if resp.payload.get(1) == Some(&RESULT_OK) {
            Ok(())
        } else {
            Err(format!("Config set failed: {:?}", resp.payload))
        }
    }

    /// Firmware sends all params in ONE report: [paramHi,paramLo,len,val...] repeated
    pub fn config_get_all(&self) -> Result<Vec<(u16, Vec<u8>)>, String> {
        self.send(PC_KB_CONFIG_GET_ALL, &[])?;
        let resp = self.recv_expect(KB_PC_CONFIG_VALUE, Duration::from_millis(1000))?;
        Ok(parse_packed_params(&resp.payload))
    }

    /// Push RGB colors using legacy protocol that firmware actually handles
    pub fn rgb_direct(&self, page: u8, colors: &[u8]) -> Result<(), String> {
        let mut payload = vec![page];
        payload.extend_from_slice(colors);
        self.send(LEGACY_RGB_DIRECT, &payload)
    }

    pub fn rgb_stop(&self) -> Result<(), String> {
        self.send(LEGACY_RGB_STOP, &[])
    }

    pub fn rgb_set_effect(&self, effect_id: u8) -> Result<(), String> {
        self.send(PC_KB_RGB_MODE, &[effect_id])
    }

    pub fn status_request(&self) -> Result<Message, String> {
        self.send(PC_KB_STATUS_REQ, &[])?;
        self.recv_expect(KB_PC_STATUS, Duration::from_millis(500))
    }

    pub fn enter_dfu(&self) -> Result<(), String> {
        self.send(PC_KB_DFU, &[])
    }

    fn recv_expect(&self, expected_cmd: u8, timeout: Duration) -> Result<Message, String> {
        let deadline = std::time::Instant::now() + timeout;
        while std::time::Instant::now() < deadline {
            let remaining = deadline.saturating_duration_since(std::time::Instant::now());
            if let Some(msg) = self.recv(remaining)? {
                if msg.cmd == expected_cmd {
                    return Ok(msg);
                }
            }
        }
        Err(format!("Timeout waiting for cmd 0x{expected_cmd:02X}"))
    }
}

/// Parse packed param format: [paramId_hi, paramId_lo, len, value...] repeated
fn parse_packed_params(data: &[u8]) -> Vec<(u16, Vec<u8>)> {
    let mut results = Vec::new();
    let mut pos = 0;
    while pos + 2 < data.len() {
        let param_id = ((data[pos] as u16) << 8) | (data[pos + 1] as u16);
        let vlen = data[pos + 2] as usize;
        pos += 3;
        if pos + vlen > data.len() {
            break;
        }
        results.push((param_id, data[pos..pos + vlen].to_vec()));
        pos += vlen;
    }
    results
}
