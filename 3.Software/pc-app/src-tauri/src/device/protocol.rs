use serde::{Deserialize, Serialize};

// ============================================================
// PC <-> Keyboard (USB HID Raw)
// Report format: [REPORT_ID=2][CMD][PAYLOAD...] (32 bytes total)
// ============================================================
pub const PC_KB_RGB_MODE: u8 = 0x01; // {effect_id:1}
pub const PC_KB_CONFIG_GET: u8 = 0x05; // {param_id_hi:1, param_id_lo:1}
pub const PC_KB_CONFIG_SET: u8 = 0x06; // {param_id_hi:1, param_id_lo:1, value:1-2}
pub const PC_KB_STATUS_REQ: u8 = 0x07;
pub const PC_KB_CONFIG_GET_ALL: u8 = 0x08;
pub const PC_KB_DFU: u8 = 0xDF; // reboot to HID bootloader

pub const KB_PC_STATUS: u8 = 0x81;
pub const KB_PC_CONFIG_VALUE: u8 = 0x82;
pub const KB_PC_ACK: u8 = 0x83;

// Legacy RGB commands — the firmware actually handles these, NOT 0x02
pub const LEGACY_RGB_DIRECT: u8 = 0xAC; // {page:1, R,G,B x10}
pub const LEGACY_RGB_STOP: u8 = 0xBD; // return control to firmware effects

// ============================================================
// Hub <-> Keyboard (UART SLIP) — for reference
// ============================================================
pub const HUB_KB_CONFIG_GET: u8 = 0x25;
pub const HUB_KB_CONFIG_SET: u8 = 0x26;
pub const HUB_KB_CONFIG_GET_ALL: u8 = 0x28;
pub const HUB_KB_KEY_ACTION: u8 = 0x29;
pub const HUB_KB_DFU: u8 = 0x2F;

// ============================================================
// PC <-> Hub (USB CDC)
// Frame format: [LEN_LO:1][LEN_HI:1][CMD:1][PAYLOAD...] (LITTLE-ENDIAN length!)
// ============================================================
pub const PC_HUB_CONFIG_GET: u8 = 0xC1;
pub const PC_HUB_CONFIG_SET: u8 = 0xC2;
pub const PC_HUB_CONFIG_GET_ALL: u8 = 0xC3;
pub const PC_HUB_STATUS_REQ: u8 = 0xC4;
pub const PC_HUB_DATA_FEED: u8 = 0xC5;
pub const PC_HUB_EINK_IMAGE: u8 = 0xC6;
pub const PC_HUB_EINK_TEXT: u8 = 0xC7;
pub const PC_HUB_FW_INFO_REQ: u8 = 0xC8;
pub const PC_HUB_DFU_KB: u8 = 0xC9;
pub const PC_HUB_DFU_HUB: u8 = 0xCA;
pub const PC_HUB_PROFILE_LIST: u8 = 0xCB;
pub const PC_HUB_PROFILE_SAVE: u8 = 0xCC;
pub const PC_HUB_PROFILE_LOAD: u8 = 0xCD;
pub const PC_HUB_PROFILE_DELETE: u8 = 0xCE;
pub const PC_HUB_APP_SWITCH: u8 = 0xD1;
pub const PC_HUB_EINK_SWITCH: u8 = 0xD2;
pub const PC_HUB_RGB_FORWARD: u8 = 0xD3;

pub const HUB_PC_CONFIG_VALUE: u8 = 0xE1;
pub const HUB_PC_STATUS: u8 = 0xE2;
pub const HUB_PC_ACK: u8 = 0xE3;
pub const HUB_PC_FW_INFO: u8 = 0xE4;
pub const HUB_PC_PROFILE_LIST: u8 = 0xE5;
pub const HUB_PC_STATE_EVENT: u8 = 0xE6;

pub const RESULT_OK: u8 = 0x00;
pub const RESULT_ERR_PARAM: u8 = 0x01;
pub const RESULT_ERR_RANGE: u8 = 0x02;
pub const RESULT_ERR_BUSY: u8 = 0x03;

pub const TARGET_KEYBOARD: u8 = 0x00;
pub const TARGET_HUB: u8 = 0x01;

// ============================================================
// Device USB identifiers — must match actual firmware descriptors
// ============================================================
pub const KB_VID: u16 = 0x1001;
pub const KB_PID: u16 = 0xF103; // keyboard main firmware
pub const KB_RAW_HID_USAGE_PAGE: u16 = 0xFFC0; // vendor-defined raw HID interface

pub const BL_VID: u16 = 0x1001;
pub const BL_PID: u16 = 0xB007; // keyboard HID bootloader
pub const BL_USAGE_PAGE: u16 = 0xFF00;

pub const HUB_VID: u16 = 0x1001;
pub const HUB_PID: u16 = 0x03EF; // hub USB CDC

// HID report sizes
pub const HID_REPORT_ID: u8 = 0x02;
pub const HID_REPORT_SIZE: usize = 33; // 1 (report id) + 32 (data)

// ============================================================
// Bootloader HID protocol (keyboard flash)
// ============================================================
pub const BL_CMD_INFO: u8 = 0x01;
pub const BL_CMD_ERASE: u8 = 0x02;
pub const BL_CMD_WRITE: u8 = 0x03;
pub const BL_CMD_SEAL: u8 = 0x04;
pub const BL_CMD_REBOOT: u8 = 0x05;

pub const BL_RSP_OK: u8 = 0x00;
pub const BL_RSP_ERR: u8 = 0x01;
pub const BL_RSP_BAD_STATE: u8 = 0x02;
pub const BL_RSP_BAD_ADDR: u8 = 0x03;
pub const BL_RSP_CRC_FAIL: u8 = 0x04;

pub const BL_WRITE_CHUNK: usize = 60;
pub const BL_REPORT_SIZE: usize = 65; // 1 (report id 0) + 64 (data)

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Message {
    pub cmd: u8,
    pub payload: Vec<u8>,
}

impl Message {
    pub fn new(cmd: u8, payload: Vec<u8>) -> Self {
        Self { cmd, payload }
    }

    /// Build HID write buffer: [REPORT_ID][CMD][PAYLOAD...] padded to 33 bytes
    pub fn to_hid_report(&self) -> Vec<u8> {
        let mut report = vec![0u8; HID_REPORT_SIZE];
        report[0] = HID_REPORT_ID;
        report[1] = self.cmd;
        let copy_len = self.payload.len().min(HID_REPORT_SIZE - 2);
        report[2..2 + copy_len].copy_from_slice(&self.payload[..copy_len]);
        report
    }

    /// Parse HID read buffer (hidapi strips report ID, so buf starts with CMD)
    pub fn from_hid_report(report: &[u8]) -> Option<Self> {
        if report.is_empty() {
            return None;
        }
        Some(Self {
            cmd: report[0],
            payload: report[1..].to_vec(),
        })
    }

    /// Build CDC frame: [LEN_LO][LEN_HI][CMD][PAYLOAD...] (LITTLE-ENDIAN length)
    pub fn to_cdc_frame(&self) -> Vec<u8> {
        let msg_len = self.payload.len() + 1; // cmd + payload
        let mut frame = Vec::with_capacity(2 + msg_len);
        frame.push((msg_len & 0xFF) as u8); // LO byte first
        frame.push((msg_len >> 8) as u8); // HI byte second
        frame.push(self.cmd);
        frame.extend_from_slice(&self.payload);
        frame
    }

    /// Parse CDC frame: [LEN_LO][LEN_HI][CMD][PAYLOAD...] (LITTLE-ENDIAN length)
    pub fn from_cdc_frame(data: &[u8]) -> Option<Self> {
        if data.len() < 3 {
            return None;
        }
        let len = (data[0] as usize) | ((data[1] as usize) << 8); // little-endian
        if data.len() < 2 + len || len == 0 {
            return None;
        }
        Some(Self {
            cmd: data[2],
            payload: data[3..2 + len].to_vec(),
        })
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum Target {
    Keyboard,
    Hub,
}

impl Target {
    pub fn to_byte(self) -> u8 {
        match self {
            Target::Keyboard => TARGET_KEYBOARD,
            Target::Hub => TARGET_HUB,
        }
    }
}
