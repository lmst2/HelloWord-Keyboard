use serde::{Deserialize, Serialize};
use std::collections::VecDeque;

/// Matches firmware: 0=error, 1=warn, 2=info, 3=debug
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DeviceLogLevel {
    Error = 0,
    Warn = 1,
    Info = 2,
    Debug = 3,
}

impl DeviceLogLevel {
    pub fn from_u8(v: u8) -> Self {
        match v.min(3) {
            0 => Self::Error,
            1 => Self::Warn,
            2 => Self::Info,
            _ => Self::Debug,
        }
    }

    pub fn as_str(&self) -> &'static str {
        match self {
            Self::Error => "error",
            Self::Warn => "warn",
            Self::Info => "info",
            Self::Debug => "debug",
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DeviceLogLine {
    pub ts_ms: u64,
    pub source: String,
    pub level: String,
    pub message: String,
}

pub struct DeviceLogStore {
    entries: VecDeque<DeviceLogLine>,
    cap: usize,
}

impl DeviceLogStore {
    pub fn new(cap: usize) -> Self {
        Self {
            entries: VecDeque::with_capacity(cap.min(512)),
            cap: cap.max(64),
        }
    }

    pub fn push(&mut self, line: DeviceLogLine) {
        if self.entries.len() >= self.cap {
            self.entries.pop_front();
        }
        self.entries.push_back(line);
    }

    pub fn snapshot(&self) -> Vec<DeviceLogLine> {
        self.entries.iter().cloned().collect()
    }

    pub fn clear(&mut self) {
        self.entries.clear();
    }
}

pub fn parse_hub_log_payload(payload: &[u8]) -> Option<DeviceLogLine> {
    if payload.len() < 3 {
        return None;
    }
    let source_byte = payload[0];
    let level_e = DeviceLogLevel::from_u8(payload[1]);
    let level = level_e.as_str().to_string();
    let message = String::from_utf8_lossy(&payload[2..]).into_owned();
    let source = match source_byte {
        crate::device::protocol::LOG_SOURCE_KEYBOARD => "keyboard",
        _ => "hub",
    }
    .to_string();
    let ts_ms = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map(|d| d.as_millis() as u64)
        .unwrap_or(0);
    Some(DeviceLogLine {
        ts_ms,
        source,
        level,
        message,
    })
}
