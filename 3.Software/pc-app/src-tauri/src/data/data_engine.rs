use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use sysinfo::System;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FeedDescriptor {
    pub feed_id: u8,
    pub name: String,
    pub unit: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FeedData {
    pub feed_id: u8,
    pub value_f32: f32,
    pub display: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProviderInfo {
    pub id: String,
    pub name: String,
    pub enabled: bool,
    pub interval_ms: u64,
    pub feeds: Vec<FeedDescriptor>,
}

pub const FEED_CPU_USAGE: u8 = 0x01;
pub const FEED_RAM_USAGE: u8 = 0x02;
pub const FEED_CPU_TEMP: u8 = 0x03;
pub const FEED_DISK_USAGE: u8 = 0x04;
pub const FEED_NET_UP: u8 = 0x05;
pub const FEED_NET_DOWN: u8 = 0x06;
pub const FEED_GPU_USAGE: u8 = 0x10;
pub const FEED_GPU_TEMP: u8 = 0x11;
pub const FEED_WEATHER_TEMP: u8 = 0x20;
pub const FEED_TIME: u8 = 0x30;

pub struct DataProviderEngine {
    sys: System,
    running: bool,
    enabled_providers: HashMap<String, bool>,
    last_poll: HashMap<u8, FeedData>,
}

impl DataProviderEngine {
    pub fn new() -> Self {
        let mut enabled = HashMap::new();
        enabled.insert("system_stats".to_string(), true);
        enabled.insert("gpu_stats".to_string(), false);
        enabled.insert("weather".to_string(), false);
        enabled.insert("clock".to_string(), true);
        Self {
            sys: System::new_all(),
            running: false,
            enabled_providers: enabled,
            last_poll: HashMap::new(),
        }
    }

    pub fn start(&mut self) {
        self.running = true;
    }

    pub fn stop(&mut self) {
        self.running = false;
    }

    pub fn is_running(&self) -> bool {
        self.running
    }

    pub fn set_provider_enabled(&mut self, id: &str, enabled: bool) {
        self.enabled_providers.insert(id.to_string(), enabled);
    }

    pub fn get_providers(&self) -> Vec<ProviderInfo> {
        vec![
            ProviderInfo {
                id: "system_stats".to_string(),
                name: "System Stats".to_string(),
                enabled: *self.enabled_providers.get("system_stats").unwrap_or(&true),
                interval_ms: 1000,
                feeds: vec![
                    FeedDescriptor { feed_id: FEED_CPU_USAGE, name: "CPU Usage".to_string(), unit: "%".to_string() },
                    FeedDescriptor { feed_id: FEED_RAM_USAGE, name: "RAM Usage".to_string(), unit: "%".to_string() },
                    FeedDescriptor { feed_id: FEED_DISK_USAGE, name: "Disk Usage".to_string(), unit: "%".to_string() },
                ],
            },
            ProviderInfo {
                id: "gpu_stats".to_string(),
                name: "GPU Stats (NVIDIA)".to_string(),
                enabled: *self.enabled_providers.get("gpu_stats").unwrap_or(&false),
                interval_ms: 1000,
                feeds: vec![
                    FeedDescriptor { feed_id: FEED_GPU_USAGE, name: "GPU Usage".to_string(), unit: "%".to_string() },
                    FeedDescriptor { feed_id: FEED_GPU_TEMP, name: "GPU Temp".to_string(), unit: "°C".to_string() },
                ],
            },
            ProviderInfo {
                id: "weather".to_string(),
                name: "Weather".to_string(),
                enabled: *self.enabled_providers.get("weather").unwrap_or(&false),
                interval_ms: 600000,
                feeds: vec![
                    FeedDescriptor { feed_id: FEED_WEATHER_TEMP, name: "Temperature".to_string(), unit: "°C".to_string() },
                ],
            },
            ProviderInfo {
                id: "clock".to_string(),
                name: "Clock / Date".to_string(),
                enabled: *self.enabled_providers.get("clock").unwrap_or(&true),
                interval_ms: 60000,
                feeds: vec![
                    FeedDescriptor { feed_id: FEED_TIME, name: "Time".to_string(), unit: "".to_string() },
                ],
            },
        ]
    }

    pub fn poll(&mut self) -> Vec<FeedData> {
        if !self.running {
            return vec![];
        }

        let mut feeds = Vec::new();

        // System stats
        if *self.enabled_providers.get("system_stats").unwrap_or(&true) {
            self.sys.refresh_cpu_usage();
            self.sys.refresh_memory();

            let cpu = self.sys.global_cpu_usage();
            let total_mem = self.sys.total_memory() as f64;
            let used_mem = self.sys.used_memory() as f64;
            let ram_pct = if total_mem > 0.0 { (used_mem / total_mem * 100.0) as f32 } else { 0.0 };

            feeds.push(FeedData {
                feed_id: FEED_CPU_USAGE,
                value_f32: cpu,
                display: format!("{cpu:.1}%"),
            });
            feeds.push(FeedData {
                feed_id: FEED_RAM_USAGE,
                value_f32: ram_pct,
                display: format!("{ram_pct:.1}%"),
            });

            let disks = sysinfo::Disks::new_with_refreshed_list();
            if let Some(disk) = disks.list().first() {
                let total = disk.total_space() as f64;
                let avail = disk.available_space() as f64;
                let used_pct = if total > 0.0 { ((total - avail) / total * 100.0) as f32 } else { 0.0 };
                feeds.push(FeedData {
                    feed_id: FEED_DISK_USAGE,
                    value_f32: used_pct,
                    display: format!("{used_pct:.1}%"),
                });
            }
        }

        // GPU stats (placeholder — needs nvml-wrapper or platform-specific)
        if *self.enabled_providers.get("gpu_stats").unwrap_or(&false) {
            feeds.push(FeedData {
                feed_id: FEED_GPU_USAGE,
                value_f32: 0.0,
                display: "N/A".to_string(),
            });
            feeds.push(FeedData {
                feed_id: FEED_GPU_TEMP,
                value_f32: 0.0,
                display: "N/A".to_string(),
            });
        }

        // Clock
        if *self.enabled_providers.get("clock").unwrap_or(&true) {
            let now = chrono_lite_time();
            feeds.push(FeedData {
                feed_id: FEED_TIME,
                value_f32: 0.0,
                display: now,
            });
        }

        for f in &feeds {
            self.last_poll.insert(f.feed_id, f.clone());
        }
        feeds
    }

    pub fn get_last_poll(&self) -> Vec<FeedData> {
        self.last_poll.values().cloned().collect()
    }

    pub fn encode_feed_for_device(feed: &FeedData) -> Vec<u8> {
        feed.value_f32.to_le_bytes().to_vec()
    }
}

fn chrono_lite_time() -> String {
    let dur = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap_or_default();
    let secs = dur.as_secs();
    let hours = (secs / 3600) % 24;
    let mins = (secs / 60) % 60;
    format!("{hours:02}:{mins:02}")
}
