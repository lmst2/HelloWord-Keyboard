//! Normalize PNGs to RGBA for `generate_context!`, and rebuild `icon.ico` from PNG layers so
//! macOS bundler can `image::open` it when merging icons (bad repo ICO caused `fill whole buffer`).
use std::borrow::Cow;
use std::fs::{rename, File};
use std::io::BufWriter;
use std::path::{Path, PathBuf};

use image::codecs::ico::{IcoEncoder, IcoFrame};
use image::codecs::png::PngEncoder;
use image::{ExtendedColorType, ImageEncoder};

fn ensure_rgba_png(manifest_dir: &Path, relative: &str) {
    let path = manifest_dir.join(relative);
    let Ok(img) = image::open(&path) else {
        return;
    };
    if img.color() == image::ColorType::Rgba8 {
        return;
    }
    let rgba = img.to_rgba8();
    let mut tmp = path.as_os_str().to_os_string();
    tmp.push(".tmp");
    let tmp_path = PathBuf::from(tmp);
    if image::DynamicImage::from(rgba).save(&tmp_path).is_err() {
        let _ = std::fs::remove_file(&tmp_path);
        return;
    }
    let _ = rename(&tmp_path, &path);
}

/// Windows-friendly ICO; also decodable by the macOS ICNS merge path in tauri-bundler.
fn rewrite_icon_ico_from_pngs(icons_dir: &Path) {
    let layers = ["32x32.png", "128x128.png", "128x128@2x.png"];
    let mut frames: Vec<IcoFrame<'static>> = Vec::new();
    for name in layers {
        let path = icons_dir.join(name);
        let Ok(img) = image::open(&path) else {
            continue;
        };
        let rgba = img.to_rgba8();
        let w = rgba.width();
        let h = rgba.height();
        if w != h || w == 0 || w > 256 {
            continue;
        }
        let mut png_bytes = Vec::new();
        if PngEncoder::new(&mut png_bytes)
            .write_image(rgba.as_raw(), w, h, ExtendedColorType::Rgba8)
            .is_err()
        {
            continue;
        }
        let Ok(frame) = IcoFrame::with_encoded(Cow::Owned(png_bytes), w, h, ExtendedColorType::Rgba8)
        else {
            continue;
        };
        frames.push(frame);
    }
    if frames.is_empty() {
        return;
    }
    let out = icons_dir.join("icon.ico");
    let mut tmp = out.as_os_str().to_os_string();
    tmp.push(".tmp");
    let tmp_path = PathBuf::from(tmp);
    let Ok(file) = File::create(&tmp_path) else {
        return;
    };
    let enc = IcoEncoder::new(BufWriter::new(file));
    if enc.encode_images(&frames).is_err() {
        let _ = std::fs::remove_file(&tmp_path);
        return;
    }
    let _ = rename(&tmp_path, &out);
}

fn main() {
    let manifest_dir = PathBuf::from(std::env::var("CARGO_MANIFEST_DIR").unwrap());
    let icons_dir = manifest_dir.join("icons");
    for rel in [
        "icons/32x32.png",
        "icons/128x128.png",
        "icons/128x128@2x.png",
        "icons/icon.png",
    ] {
        ensure_rgba_png(&manifest_dir, rel);
    }
    rewrite_icon_ico_from_pngs(&icons_dir);
    tauri_build::build()
}
