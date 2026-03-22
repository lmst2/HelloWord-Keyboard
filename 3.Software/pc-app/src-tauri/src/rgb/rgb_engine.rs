use super::color_utils::{lerp_color, Rgb};
use serde::{Deserialize, Serialize};

pub const NUM_LEDS: usize = 104;
pub const LEDS_PER_PAGE: usize = 10;
pub const NUM_PAGES: usize = 11;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum RgbMode {
    Off,
    CpuTempGradient {
        cool_color: Rgb,
        hot_color: Rgb,
        temp_range: (f32, f32),
    },
    StaticColor(Rgb),
}

pub struct RgbEngine {
    mode: RgbMode,
    running: bool,
    led_buffer: Vec<Rgb>,
    current_temp: f32,
}

impl RgbEngine {
    pub fn new() -> Self {
        Self {
            mode: RgbMode::Off,
            running: false,
            led_buffer: vec![Rgb::new(0, 0, 0); NUM_LEDS],
            current_temp: 40.0,
        }
    }

    pub fn get_mode(&self) -> &RgbMode {
        &self.mode
    }

    pub fn set_mode(&mut self, mode: RgbMode) {
        self.mode = mode;
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

    pub fn update_cpu_temp(&mut self, temp: f32) {
        self.current_temp = temp;
    }

    /// Render one frame, returns page data for LEGACY_RGB_DIRECT if active
    pub fn render(&mut self) -> Option<Vec<(u8, Vec<u8>)>> {
        if !self.running {
            return None;
        }
        match &self.mode {
            RgbMode::Off => None,
            RgbMode::CpuTempGradient {
                cool_color,
                hot_color,
                temp_range,
            } => {
                let t = ((self.current_temp - temp_range.0)
                    / (temp_range.1 - temp_range.0))
                    .clamp(0.0, 1.0);
                let color = lerp_color(*cool_color, *hot_color, t);
                for led in &mut self.led_buffer {
                    *led = color;
                }
                Some(self.build_pages())
            }
            RgbMode::StaticColor(color) => {
                for led in &mut self.led_buffer {
                    *led = *color;
                }
                Some(self.build_pages())
            }
        }
    }

    /// Build page data: Vec<(page_index, [R,G,B x 10])>
    fn build_pages(&self) -> Vec<(u8, Vec<u8>)> {
        let mut pages = Vec::with_capacity(NUM_PAGES);
        for page_idx in 0..NUM_PAGES {
            let start = page_idx * LEDS_PER_PAGE;
            let end = (start + LEDS_PER_PAGE).min(NUM_LEDS);
            let mut data = Vec::with_capacity(LEDS_PER_PAGE * 3);
            for i in start..end {
                data.push(self.led_buffer[i].r);
                data.push(self.led_buffer[i].g);
                data.push(self.led_buffer[i].b);
            }
            while data.len() < LEDS_PER_PAGE * 3 {
                data.push(0);
            }
            pages.push((page_idx as u8, data));
        }
        pages
    }
}
