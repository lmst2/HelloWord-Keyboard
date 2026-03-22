use crate::device::protocol::Target;
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum ParamType {
    U8,
    U16,
    Bool,
    Enum,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum ParamValue {
    U8(u8),
    U16(u16),
    Bool(bool),
}

impl ParamValue {
    pub fn to_bytes(&self) -> Vec<u8> {
        match self {
            ParamValue::U8(v) => vec![*v],
            ParamValue::U16(v) => v.to_be_bytes().to_vec(),
            ParamValue::Bool(v) => vec![if *v { 1 } else { 0 }],
        }
    }

    pub fn from_bytes(ptype: &ParamType, data: &[u8]) -> Option<Self> {
        match ptype {
            ParamType::U8 | ParamType::Enum => data.first().map(|&v| ParamValue::U8(v)),
            ParamType::U16 => {
                if data.len() >= 2 {
                    Some(ParamValue::U16(u16::from_be_bytes([data[0], data[1]])))
                } else {
                    None
                }
            }
            ParamType::Bool => data.first().map(|&v| ParamValue::Bool(v != 0)),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ParamMeta {
    pub id: u16,
    pub name: String,
    pub category: String,
    pub target: Target,
    pub value_type: ParamType,
    pub min: i32,
    pub max: i32,
    pub step: i32,
    pub enum_labels: Option<Vec<String>>,
    pub default_value: Vec<u8>,
}

pub struct ParamRegistry {
    params: Vec<ParamMeta>,
}

impl ParamRegistry {
    pub fn new() -> Self {
        Self {
            params: build_param_table(),
        }
    }

    pub fn get_all(&self) -> &[ParamMeta] {
        &self.params
    }

    pub fn get(&self, id: u16) -> Option<&ParamMeta> {
        self.params.iter().find(|p| p.id == id)
    }

    pub fn get_by_category(&self, category: &str) -> Vec<&ParamMeta> {
        self.params
            .iter()
            .filter(|p| p.category == category)
            .collect()
    }

    pub fn get_categories(&self) -> Vec<String> {
        let mut cats: Vec<String> = self.params.iter().map(|p| p.category.clone()).collect();
        cats.sort();
        cats.dedup();
        cats
    }
}

fn pm(
    id: u16,
    name: &str,
    cat: &str,
    target: Target,
    vtype: ParamType,
    min: i32,
    max: i32,
    step: i32,
    labels: Option<Vec<&str>>,
    default: Vec<u8>,
) -> ParamMeta {
    ParamMeta {
        id,
        name: name.to_string(),
        category: cat.to_string(),
        target,
        value_type: vtype,
        min,
        max,
        step,
        enum_labels: labels.map(|l| l.into_iter().map(String::from).collect()),
        default_value: default,
    }
}

fn build_param_table() -> Vec<ParamMeta> {
    use ParamType::*;
    use Target::*;
    vec![
        // Lighting
        pm(0x0100, "Effect Mode", "Lighting", Keyboard, Enum, 0, 7, 1,
           Some(vec!["Rainbow Sweep", "Reactive", "Aurora", "Ripple", "Static", "Breathing", "Wave", "Off"]),
           vec![0]),
        pm(0x0101, "Brightness", "Lighting", Keyboard, U8, 0, 6, 1, None, vec![4]),
        pm(0x0106, "Effect Speed", "Lighting", Keyboard, U8, 0, 255, 1, None, vec![128]),
        pm(0x0110, "Rainbow Hue Offset", "Lighting / Effect Colors", Keyboard, U8, 0, 255, 1, None, vec![0]),
        pm(0x0120, "Reactive Hue", "Lighting / Effect Colors", Keyboard, U8, 0, 255, 1, None, vec![128]),
        pm(0x0121, "Reactive Saturation", "Lighting / Effect Colors", Keyboard, U8, 0, 255, 1, None, vec![200]),
        pm(0x0130, "Aurora Tint Hue", "Lighting / Effect Colors", Keyboard, U8, 0, 255, 1, None, vec![110]),
        pm(0x0140, "Ripple Hue", "Lighting / Effect Colors", Keyboard, U8, 0, 255, 1, None, vec![0]),
        pm(0x0150, "Static Red", "Lighting / Effect Colors", Keyboard, U8, 0, 255, 1, None, vec![255]),
        pm(0x0151, "Static Green", "Lighting / Effect Colors", Keyboard, U8, 0, 255, 1, None, vec![180]),
        pm(0x0152, "Static Blue", "Lighting / Effect Colors", Keyboard, U8, 0, 255, 1, None, vec![80]),
        // TouchBar
        pm(0x0200, "TouchBar Mode", "TouchBar", Keyboard, Enum, 0, 3, 1,
           Some(vec!["Off", "Mouse Wheel", "Volume", "Brightness"]),
           vec![0]),
        pm(0x0201, "Activation Time (ms)", "TouchBar", Keyboard, U16, 5, 100, 5, None, vec![0, 20]),
        pm(0x0202, "Release Grace (ms)", "TouchBar", Keyboard, U16, 10, 200, 5, None, vec![0, 35]),
        // Keymap
        pm(0x0300, "Active Layer", "Keymap", Keyboard, Enum, 0, 3, 1,
           Some(vec!["Layer 0", "Layer 1", "Layer 2", "Layer 3"]),
           vec![1]),
        pm(0x0301, "OS Mode", "Keymap", Keyboard, Enum, 0, 2, 1,
           Some(vec!["Windows", "macOS", "Auto"]),
           vec![0xFF]),
        // Sleep
        pm(0x0400, "Sleep Timeout (min)", "Sleep", Keyboard, U8, 1, 60, 1, None, vec![5]),
        pm(0x0401, "Fade Duration (ms)", "Sleep", Keyboard, U16, 100, 5000, 100, None, vec![3, 32]),
        pm(0x0402, "Breathe Period (ms)", "Sleep", Keyboard, U16, 1000, 10000, 500, None, vec![18, 192]),
        // Hub general
        pm(0x0500, "OLED Brightness", "Hub / General", Hub, U8, 0, 255, 1, None, vec![128]),
        pm(0x0501, "Standby Timeout (s)", "Hub / General", Hub, U16, 10, 3600, 10, None, vec![0, 60]),
        pm(0x0503, "Heartbeat Interval (s)", "Hub / General", Hub, U8, 1, 60, 1, None, vec![5]),
        // Hub motor
        pm(0x0510, "Default Torque Limit", "Hub / Motor", Hub, U8, 1, 100, 1, None, vec![50]),
        pm(0x0511, "Default PPR", "Hub / Motor", Hub, U8, 1, 48, 1, None, vec![24]),
        // Hub apps
        pm(0x0520, "Primary App ID", "Hub / Apps", Hub, U8, 0, 23, 1, None, vec![0]),
        pm(0x0521, "E-Ink App ID", "Hub / Apps", Hub, U8, 0, 23, 1, None, vec![0]),
    ]
}
