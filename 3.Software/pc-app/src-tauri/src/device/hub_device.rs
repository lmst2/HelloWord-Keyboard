use super::protocol::*;
use serialport::SerialPort;
use std::io::{Read, Write};
use std::time::Duration;

pub struct HubDevice {
    port: Box<dyn SerialPort>,
    read_buf: Vec<u8>,
}

impl HubDevice {
    pub fn new(port: Box<dyn SerialPort>) -> Self {
        Self {
            port,
            read_buf: Vec::with_capacity(4096),
        }
    }

    pub fn send(&mut self, cmd: u8, payload: &[u8]) -> Result<(), String> {
        self.send_impl(cmd, payload, true)
    }

    /// Hub reboots immediately on PC_HUB_DFU_HUB; FlushFileBuffers can block forever on Windows after USB disconnect.
    pub fn send_without_flush(&mut self, cmd: u8, payload: &[u8]) -> Result<(), String> {
        self.send_impl(cmd, payload, false)
    }

    fn send_impl(&mut self, cmd: u8, payload: &[u8], flush: bool) -> Result<(), String> {
        let msg = Message::new(cmd, payload.to_vec());
        let frame = msg.to_cdc_frame(); // uses little-endian length
        // Windows usbser CDC often rejects single large WriteFile (os error 22 / EINVAL).
        const CHUNK: usize = 64;
        for chunk in frame.chunks(CHUNK) {
            self.port
                .write_all(chunk)
                .map_err(|e| format!("Serial write error: {e}"))?;
        }
        if flush {
            self.port
                .flush()
                .map_err(|e| format!("Serial flush error: {e}"))?;
        }
        Ok(())
    }

    pub fn recv(&mut self, timeout: Duration) -> Result<Option<Message>, String> {
        self.port
            .set_timeout(timeout)
            .map_err(|e| format!("Set timeout error: {e}"))?;

        let mut tmp = [0u8; 512];
        match self.port.read(&mut tmp) {
            Ok(n) if n > 0 => self.read_buf.extend_from_slice(&tmp[..n]),
            Err(ref e) if e.kind() == std::io::ErrorKind::TimedOut => {}
            Err(e) => return Err(format!("Serial read error: {e}")),
            _ => {}
        }

        if let Some(msg) = self.try_parse_frame() {
            return Ok(Some(msg));
        }
        Ok(None)
    }

    /// Parse little-endian length-prefixed CDC frame
    fn try_parse_frame(&mut self) -> Option<Message> {
        if self.read_buf.len() < 3 {
            return None;
        }
        // LITTLE-ENDIAN: [len_lo, len_hi]
        let frame_len = (self.read_buf[0] as usize) | ((self.read_buf[1] as usize) << 8);
        if frame_len == 0 || self.read_buf.len() < 2 + frame_len {
            return None;
        }
        let cmd = self.read_buf[2];
        let payload = self.read_buf[3..2 + frame_len].to_vec();
        self.read_buf.drain(..2 + frame_len);
        Some(Message::new(cmd, payload))
    }

    pub fn config_get(&mut self, target: Target, param: u16) -> Result<Vec<u8>, String> {
        let payload = vec![target.to_byte(), (param >> 8) as u8, (param & 0xFF) as u8];
        self.send(PC_HUB_CONFIG_GET, &payload)?;
        let resp = self.recv_expect(HUB_PC_CONFIG_VALUE, Duration::from_millis(1000))?;
        // resp.payload = [target, paramHi, paramLo, value...]
        if resp.payload.len() >= 3 {
            Ok(resp.payload[3..].to_vec())
        } else {
            Err("Invalid config value response".into())
        }
    }

    pub fn config_set(
        &mut self,
        target: Target,
        param: u16,
        value: &[u8],
    ) -> Result<(), String> {
        let mut payload = vec![target.to_byte(), (param >> 8) as u8, (param & 0xFF) as u8];
        payload.extend_from_slice(value);
        self.send(PC_HUB_CONFIG_SET, &payload)?;
        let resp = self.recv_expect(HUB_PC_ACK, Duration::from_millis(1000))?;
        // resp.payload = [cmd_echo, result]
        if resp.payload.get(1) == Some(&RESULT_OK) {
            Ok(())
        } else {
            Err(format!("Hub config set failed: {:?}", resp.payload))
        }
    }

    pub fn data_feed(&mut self, feed_id: u8, data: &[u8]) -> Result<(), String> {
        let mut payload = vec![feed_id];
        payload.extend_from_slice(data);
        self.send(PC_HUB_DATA_FEED, &payload)
    }

    pub fn eink_upload(&mut self, slot: u8, page: u8, data: &[u8]) -> Result<(), String> {
        let mut payload = vec![slot, page];
        payload.extend_from_slice(data);
        self.send(PC_HUB_EINK_IMAGE, &payload)
    }

    pub fn eink_send_text(&mut self, text: &str) -> Result<(), String> {
        self.send(PC_HUB_EINK_TEXT, text.as_bytes())
    }

    pub fn request_fw_info(&mut self) -> Result<Message, String> {
        self.send(PC_HUB_FW_INFO_REQ, &[])?;
        self.recv_expect(HUB_PC_FW_INFO, Duration::from_millis(1000))
    }

    pub fn profile_list(&mut self) -> Result<Message, String> {
        self.send(PC_HUB_PROFILE_LIST, &[])?;
        self.recv_expect(HUB_PC_PROFILE_LIST, Duration::from_millis(1000))
    }

    pub fn profile_save(&mut self, slot: u8, name: &str) -> Result<(), String> {
        let mut payload = vec![slot];
        payload.extend_from_slice(name.as_bytes());
        self.send(PC_HUB_PROFILE_SAVE, &payload)?;
        let resp = self.recv_expect(HUB_PC_ACK, Duration::from_millis(2000))?;
        if resp.payload.get(1) == Some(&RESULT_OK) {
            Ok(())
        } else {
            Err("Profile save failed".into())
        }
    }

    pub fn profile_load(&mut self, slot: u8) -> Result<(), String> {
        self.send(PC_HUB_PROFILE_LOAD, &[slot])?;
        let resp = self.recv_expect(HUB_PC_ACK, Duration::from_millis(2000))?;
        if resp.payload.get(1) == Some(&RESULT_OK) {
            Ok(())
        } else {
            Err("Profile load failed".into())
        }
    }

    pub fn profile_delete(&mut self, slot: u8) -> Result<(), String> {
        self.send(PC_HUB_PROFILE_DELETE, &[slot])?;
        let resp = self.recv_expect(HUB_PC_ACK, Duration::from_millis(1000))?;
        if resp.payload.get(1) == Some(&RESULT_OK) {
            Ok(())
        } else {
            Err("Profile delete failed".into())
        }
    }

    pub fn switch_primary_app(&mut self, app_id: u8) -> Result<(), String> {
        self.send(PC_HUB_APP_SWITCH, &[app_id])
    }

    pub fn switch_eink_app(&mut self, app_id: u8) -> Result<(), String> {
        self.send(PC_HUB_EINK_SWITCH, &[app_id])
    }

    pub fn enter_dfu_kb(&mut self) -> Result<(), String> {
        self.send(PC_HUB_DFU_KB, &[])
    }

    pub fn enter_dfu_hub(&mut self) -> Result<(), String> {
        self.send_without_flush(PC_HUB_DFU_HUB, &[])
    }

    /// Forward RGB commands to keyboard through Hub's UART relay
    pub fn rgb_forward(&mut self, rgb_cmd: u8, data: &[u8]) -> Result<(), String> {
        let mut payload = vec![rgb_cmd];
        payload.extend_from_slice(data);
        self.send(PC_HUB_RGB_FORWARD, &payload)
    }

    fn recv_expect(&mut self, expected_cmd: u8, timeout: Duration) -> Result<Message, String> {
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
