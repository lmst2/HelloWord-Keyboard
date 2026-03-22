use crate::data::DataProviderEngine;
use crate::device::DeviceManager;
use crate::rgb::RgbEngine;
use std::sync::Arc;
use tauri::{
    menu::{Menu, MenuItem},
    tray::TrayIconBuilder,
    App, Manager,
};
// Single tray from Rust only — tauri.conf trayIcon duplicates an icon on Windows (blank + broken color tile).
use tokio::sync::{Mutex, RwLock};

pub fn setup_tray(app: &App) -> Result<(), Box<dyn std::error::Error>> {
    let open = MenuItem::with_id(app, "open", "Open Dashboard", true, None::<&str>)?;
    let data_toggle =
        MenuItem::with_id(app, "data_toggle", "Data Feed: Start", true, None::<&str>)?;
    let quit = MenuItem::with_id(app, "quit", "Quit", true, None::<&str>)?;
    let menu = Menu::with_items(app, &[&open, &data_toggle, &quit])?;

    let mut builder = TrayIconBuilder::new()
        .menu(&menu)
        .tooltip("HelloWord-75 Manager")
        .on_menu_event(move |app, event| match event.id.as_ref() {
            "open" => {
                if let Some(window) = app.get_webview_window("main") {
                    let _ = window.show();
                    let _ = window.set_focus();
                }
            }
            "quit" => {
                app.exit(0);
            }
            _ => {}
        });
    if let Some(icon) = app.default_window_icon() {
        builder = builder.icon(icon.clone());
    }
    let _tray = builder.build(app)?;

    Ok(())
}

/// Background service: pushes data feeds and RGB frames to devices.
/// Runs as a Tokio task, keeps going even when the window is hidden.
pub fn spawn_background_services(
    device_mgr: Arc<Mutex<DeviceManager>>,
    data_engine: Arc<RwLock<DataProviderEngine>>,
    rgb_engine: Arc<RwLock<RgbEngine>>,
) {
    // Data push loop (~1 Hz)
    let dm_data = device_mgr.clone();
    let de = data_engine.clone();
    tokio::spawn(async move {
        loop {
            tokio::time::sleep(std::time::Duration::from_millis(1000)).await;
            let feeds = {
                let mut engine = de.write().await;
                engine.poll()
            };
            if feeds.is_empty() {
                continue;
            }
            let mut dm = dm_data.lock().await;
            if !dm.is_hub_connected() {
                continue;
            }
            for feed in &feeds {
                let encoded = DataProviderEngine::encode_feed_for_device(feed);
                if let Err(e) = dm.hub_data_feed(feed.feed_id, &encoded) {
                    log::warn!(
                        "Background data_feed: id=0x{:02X} len={} err={}",
                        feed.feed_id,
                        encoded.len(),
                        e
                    );
                }
            }
        }
    });

    // RGB push loop (~30 fps = 33ms)
    let dm_rgb = device_mgr;
    let re = rgb_engine;
    tokio::spawn(async move {
        loop {
            tokio::time::sleep(std::time::Duration::from_millis(33)).await;
            let pages = {
                let mut engine = re.write().await;
                engine.render()
            };
            if let Some(pages) = pages {
                let dm = dm_rgb.lock().await;
                if !dm.is_keyboard_connected() {
                    continue;
                }
                for (page_idx, colors) in &pages {
                    if let Err(e) = dm.kb_rgb_direct(*page_idx, colors) {
                        log::trace!(
                            "Background rgb_direct: page={} colors_len={} err={}",
                            page_idx,
                            colors.len(),
                            e
                        );
                    }
                }
            }
        }
    });
}
