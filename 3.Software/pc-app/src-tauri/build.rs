//! Tauri icon checks require RGBA PNGs; normalize before `tauri_build`.
fn ensure_rgba_png(manifest_dir: &std::path::Path, relative: &str) {
    let path = manifest_dir.join(relative);
    let Ok(img) = image::open(&path) else {
        return;
    };
    let rgba = img.to_rgba8();
    let _ = image::DynamicImage::from(rgba).save(&path);
}

fn main() {
    let manifest_dir = std::path::PathBuf::from(std::env::var("CARGO_MANIFEST_DIR").unwrap());
    for rel in [
        "icons/32x32.png",
        "icons/128x128.png",
        "icons/128x128@2x.png",
        "icons/icon.png",
    ] {
        ensure_rgba_png(&manifest_dir, rel);
    }
    tauri_build::build()
}
