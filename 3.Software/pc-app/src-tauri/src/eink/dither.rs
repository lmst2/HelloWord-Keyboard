/// Floyd-Steinberg dithering: grayscale image -> 1-bit monochrome packed bytes
pub fn floyd_steinberg_dither(width: u32, height: u32, gray: &[u8]) -> Vec<u8> {
    let w = width as usize;
    let h = height as usize;
    let mut errors: Vec<f32> = gray.iter().map(|&v| v as f32).collect();

    for y in 0..h {
        for x in 0..w {
            let idx = y * w + x;
            let old = errors[idx];
            let new_val = if old > 127.0 { 255.0 } else { 0.0 };
            let err = old - new_val;
            errors[idx] = new_val;

            if x + 1 < w {
                errors[idx + 1] += err * 7.0 / 16.0;
            }
            if y + 1 < h {
                if x > 0 {
                    errors[(y + 1) * w + x - 1] += err * 3.0 / 16.0;
                }
                errors[(y + 1) * w + x] += err * 5.0 / 16.0;
                if x + 1 < w {
                    errors[(y + 1) * w + x + 1] += err * 1.0 / 16.0;
                }
            }
        }
    }

    // Pack into bytes (MSB first)
    let row_bytes = (w + 7) / 8;
    let mut packed = vec![0u8; row_bytes * h];
    for y in 0..h {
        for x in 0..w {
            if errors[y * w + x] > 127.0 {
                packed[y * row_bytes + x / 8] |= 0x80 >> (x % 8);
            }
        }
    }
    packed
}
