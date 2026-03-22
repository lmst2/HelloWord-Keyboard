use super::dither::floyd_steinberg_dither;

pub const EINK_WIDTH: u32 = 128;
pub const EINK_HEIGHT: u32 = 296;
const PAGE_SIZE: usize = 128;

pub struct EinkPipeline;

impl EinkPipeline {
    pub fn new() -> Self {
        Self
    }

    pub fn prepare_image(&self, image_bytes: &[u8]) -> Result<Vec<u8>, String> {
        let img = image::load_from_memory(image_bytes)
            .map_err(|e| format!("Failed to decode image: {e}"))?;
        let resized = img.resize_exact(EINK_WIDTH, EINK_HEIGHT, image::imageops::FilterType::Lanczos3);
        let gray = resized.to_luma8();
        Ok(floyd_steinberg_dither(EINK_WIDTH, EINK_HEIGHT, gray.as_raw()))
    }

    pub fn split_into_pages(data: &[u8]) -> Vec<(u8, Vec<u8>)> {
        let mut pages = Vec::new();
        let mut offset = 0;
        let mut page_num: u8 = 0;
        while offset < data.len() {
            let end = (offset + PAGE_SIZE).min(data.len());
            pages.push((page_num, data[offset..end].to_vec()));
            offset = end;
            page_num = page_num.wrapping_add(1);
        }
        pages
    }
}
