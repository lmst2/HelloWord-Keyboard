use super::protocol::*;
use serialport::SerialPort;
use std::io::{Read, Write};
use std::time::Duration;

pub struct HubDevice {
    port: Box<dyn SerialPort>,
    read_buf: Vec<u8>,
    /// Messages not consumed by last recv_expect (e.g. HUB_PC_STATE_EVENT) — do not drop on mismatch.
    pending: Vec<Message>,
}

impl HubDevice {
    pub fn new(mut port: Box<dyn SerialPort>) -> Self {
        // STM32 USB CDC on Windows often will not IN-transfer until host asserts DTR.
        let _ = port.write_data_terminal_ready(true);
        let _ = port.write_request_to_send(true);
        let _ = port.clear(serialport::ClearBuffer::All);
        Self {
            port,
            read_buf: Vec::with_capacity(4096),
            pending: Vec::new(),
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
        if cmd == PC_HUB_RGB_FORWARD
            || cmd == PC_HUB_DATA_FEED
            || cmd == PC_HUB_LOG_CONFIG
        {
            log::trace!(
                "Hub TX {} (0x{:02X}) payload_len={} flush={}",
                cdc_cmd_label(cmd),
                cmd,
                payload.len(),
                flush
            );
        } else {
            log::info!(
                "Hub TX {} (0x{:02X}) payload_len={} flush={}",
                cdc_cmd_label(cmd),
                cmd,
                payload.len(),
                flush
            );
        }
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
            Ok(n) if n > 0 => {
                log::trace!("Hub serial read {} bytes from CDC", n);
                self.read_buf.extend_from_slice(&tmp[..n]);
            }
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
        self.recv_expect(
            HUB_PC_PROFILE_LIST,
            Duration::from_millis(8000),
        )
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
        self.send(PC_HUB_EINK_SWITCH, &[app_id])?;
        match self.recv_expect(HUB_PC_ACK, Duration::from_millis(2000)) {
            Ok(resp) => {
                if resp.payload.first() == Some(&PC_HUB_EINK_SWITCH)
                    && resp.payload.get(1) == Some(&RESULT_OK)
                {
                    log::info!("Hub e-ink switch ACK ok app_id=0x{app_id:02X}");
                    Ok(())
                } else {
                    Err(format!("Hub e-ink switch rejected: {:?}", resp.payload))
                }
            }
            Err(e) => {
                // Command was written; many stacks need DTR for device TX — see HubDevice::new.
                // Old Hub FW may also omit CDC replies.
                log::warn!(
                    "Hub e-ink switch: no ACK (command was sent): {} — check Hub Dynamic-fw + CDC RX",
                    e
                );
                Ok(())
            }
        }
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

    /// Push unsolicited CDC messages (e.g. log lines) for later recv_expect consumption.
    pub fn push_pending(&mut self, msg: Message) {
        const PENDING_CAP: usize = 64;
        if self.pending.len() < PENDING_CAP {
            self.pending.push(msg);
        }
    }

    /// Non-blocking drain: routes `HUB_PC_LOG` to `out_logs`, everything else to `pending`.
    pub fn drain_unsolicited(
        &mut self,
        out_logs: &mut Vec<crate::device_log::DeviceLogLine>,
    ) -> Result<(), String> {
        loop {
            match self.recv(Duration::from_millis(1)) {
                Ok(Some(m)) => {
                    if m.cmd == HUB_PC_LOG {
                        if let Some(line) = crate::device_log::parse_hub_log_payload(&m.payload) {
                            out_logs.push(line);
                        }
                    } else {
                        self.push_pending(m);
                    }
                }
                Ok(None) => break,
                Err(e) => return Err(e),
            }
        }
        Ok(())
    }

    pub fn log_config(&mut self, enabled: bool, max_level: u8) -> Result<(), String> {
        let ml = max_level.min(3);
        self.send(
            PC_HUB_LOG_CONFIG,
            &[if enabled { 1u8 } else { 0u8 }, ml],
        )
    }

    fn recv_expect(&mut self, expected_cmd: u8, timeout: Duration) -> Result<Message, String> {
        const PENDING_CAP: usize = 64;
        let deadline = std::time::Instant::now() + timeout;
        loop {
            if let Some(i) = self
                .pending
                .iter()
                .position(|m| m.cmd == expected_cmd)
            {
                let msg = self.pending.remove(i);
                log::debug!(
                    "Hub RX {} (0x{:02X}) payload_len={} (from pending)",
                    cdc_cmd_label(msg.cmd),
                    msg.cmd,
                    msg.payload.len()
                );
                return Ok(msg);
            }
            if std::time::Instant::now() >= deadline {
                break;
            }
            let remaining = deadline.saturating_duration_since(std::time::Instant::now());
            if let Some(msg) = self.recv(remaining)? {
                if msg.cmd == expected_cmd {
                    log::debug!(
                        "Hub RX {} (0x{:02X}) payload_len={} (matched wait)",
                        cdc_cmd_label(msg.cmd),
                        msg.cmd,
                        msg.payload.len()
                    );
                    return Ok(msg);
                }
                log::trace!(
                    "Hub RX {} (0x{:02X}) while waiting for {} — queued pending",
                    cdc_cmd_label(msg.cmd),
                    msg.cmd,
                    cdc_cmd_label(expected_cmd)
                );
                if self.pending.len() < PENDING_CAP {
                    self.pending.push(msg);
                }
            }
        }
        log::warn!(
            "Hub RX timeout waiting {} (0x{:02X}), pending_queue_len={}, read_buf_len={} (no CDC bytes? try DTR on open, reflash HelloWord-Dynamic-fw Hub)",
            cdc_cmd_label(expected_cmd),
            expected_cmd,
            self.pending.len(),
            self.read_buf.len()
        );
        Err(format!("Timeout waiting for cmd 0x{expected_cmd:02X}"))
    }
}
