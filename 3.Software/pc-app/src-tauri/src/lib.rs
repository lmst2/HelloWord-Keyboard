mod commands;
mod config;
mod data;
mod device;
mod dfu;
mod eink;
mod profile;
mod rgb;
mod settings;
mod state;
mod tray;

use state::AppState;
use std::sync::Arc;
use tauri::Manager;
use tokio::sync::RwLock;

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    env_logger::init();

    let app_state = Arc::new(RwLock::new(AppState::new()));

    tauri::Builder::default()
        .plugin(tauri_plugin_shell::init())
        .manage(app_state.clone())
        .invoke_handler(tauri::generate_handler![
            commands::device_commands::get_device_status,
            commands::device_commands::start_discovery,
            commands::device_commands::stop_discovery,
            commands::config_commands::config_get,
            commands::config_commands::config_set,
            commands::config_commands::config_sync_all,
            commands::config_commands::get_param_registry,
            commands::rgb_commands::rgb_set_mode,
            commands::rgb_commands::rgb_get_mode,
            commands::rgb_commands::rgb_stop,
            commands::data_commands::data_start,
            commands::data_commands::data_stop,
            commands::data_commands::data_get_providers,
            commands::data_commands::data_set_provider_enabled,
            commands::data_commands::data_get_live,
            commands::eink_commands::eink_upload_image,
            commands::eink_commands::eink_send_text,
            commands::eink_commands::eink_switch_app,
            commands::profile_commands::profile_list,
            commands::profile_commands::profile_save,
            commands::profile_commands::profile_load,
            commands::profile_commands::profile_delete,
            commands::profile_commands::profile_export,
            commands::profile_commands::profile_import,
            commands::dfu_commands::dfu_get_info,
            commands::dfu_commands::dfu_flash_keyboard,
            commands::dfu_commands::dfu_flash_hub,
            commands::dfu_commands::dfu_get_progress,
            commands::settings_commands::settings_get,
            commands::settings_commands::settings_set,
        ])
        .setup(move |app| {
            tray::setup_tray(app)?;

            // Spawn background push services (Data ~1Hz, RGB ~30fps)
            let state_for_bg = app_state.clone();
            let rt = tokio::runtime::Handle::current();
            rt.spawn(async move {
                let s = state_for_bg.read().await;
                tray::spawn_background_services(
                    s.device_mgr.clone(),
                    s.data_engine.clone(),
                    s.rgb_engine.clone(),
                );
            });

            // On window close: minimize to tray instead of quitting
            let window = app.get_webview_window("main").unwrap();
            window.on_window_event(move |event| {
                if let tauri::WindowEvent::CloseRequested { api, .. } = event {
                    api.prevent_close();
                    // Window hides; background services keep running
                    // Tray "Open Dashboard" re-shows the window
                }
            });

            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
