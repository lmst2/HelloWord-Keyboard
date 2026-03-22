mod commands;
mod config;
mod data;
mod device;
mod device_log;
mod dfu;
mod eink;
mod logging;
mod profile;
mod rgb;
mod settings;
mod state;
mod tray;

use settings::AppSettings;
use state::AppState;
use std::io::Write;
use std::sync::Arc;
use tauri::Emitter;
use tauri::Manager;
use tokio::sync::RwLock;

/// GUI Windows apps use subsystem WINDOWS — stderr is not attached to PowerShell, so logs vanish.
/// We mirror log output to a file under LocalAppData and capture panics there too.
fn init_diagnostics() {
    let log_path = dirs::data_local_dir().map(|d| d.join("helloword-manager").join("app.log"));

    if let Some(ref path) = log_path {
        if let Some(parent) = path.parent() {
            let _ = std::fs::create_dir_all(parent);
        }
        let path_for_hook = path.clone();
        std::panic::set_hook(Box::new(move |info| {
            let line = format!("{info}\n");
            let _ = std::fs::OpenOptions::new()
                .create(true)
                .append(true)
                .open(&path_for_hook)
                .and_then(|mut f| f.write_all(line.as_bytes()));
            let _ = std::io::stderr().write_all(line.as_bytes());
        }));
    }

    #[cfg(windows)]
    {
        use env_logger::{Builder, Target};

        let mut builder = Builder::from_default_env();
        if let Some(ref path) = log_path {
            if let Ok(file) = std::fs::OpenOptions::new()
                .create(true)
                .append(true)
                .open(path)
            {
                struct LogTee(Mutex<std::fs::File>);
                impl Write for LogTee {
                    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
                        let n = buf.len();
                        let _ = std::io::stderr().write_all(buf);
                        self.0.lock().unwrap().write_all(buf)?;
                        Ok(n)
                    }
                    fn flush(&mut self) -> std::io::Result<()> {
                        let _ = std::io::stderr().flush();
                        self.0.lock().unwrap().flush()
                    }
                }
                builder.target(Target::Pipe(Box::new(LogTee(Mutex::new(file)))));
            }
        }
        let _ = builder.try_init();
    }
    #[cfg(not(windows))]
    {
        let _ = env_logger::Builder::from_default_env().try_init();
    }

    if let Some(ref p) = log_path {
        let rust_log = std::env::var("RUST_LOG").unwrap_or_else(|_| "<unset>".into());
        let _ = std::fs::OpenOptions::new()
            .create(true)
            .append(true)
            .open(p)
            .and_then(|mut f| {
                writeln!(f)?;
                writeln!(
                    f,
                    "===== helloword-manager start pid={} =====",
                    std::process::id()
                )?;
                writeln!(f, "RUST_LOG={}", rust_log)?;
                writeln!(f, "Log path: {}", p.display())?;
                Ok(())
            });
        log::info!("Log file: {}", p.display());
    }
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    init_diagnostics();

    let boot_settings = AppSettings::load_or_default();
    logging::apply_pc_rust_log_level(&boot_settings.pc_app_log_level);

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
            commands::log_commands::log_get_snapshot,
            commands::log_commands::log_clear,
            commands::log_commands::log_sync_device,
        ])
        .setup(move |app| {
            tray::setup_tray(app)?;

            let hub_log_pump_handle = app.handle().clone();
            let hub_log_pump_state = app_state.clone();
            tauri::async_runtime::spawn(async move {
                loop {
                    tokio::time::sleep(std::time::Duration::from_millis(25)).await;
                    let mut batch = Vec::new();
                    {
                        let s = hub_log_pump_state.read().await;
                        let mut dm = s.device_mgr.lock().await;
                        if let Err(e) = dm.hub_drain_logs(&mut batch) {
                            log::trace!("hub_drain_logs: {e}");
                        }
                    }
                    if batch.is_empty() {
                        continue;
                    }
                    for line in batch {
                        let log_store = {
                            let s = hub_log_pump_state.read().await;
                            s.device_log_store.clone()
                        };
                        if let Ok(mut store) = log_store.lock() {
                            store.push(line.clone());
                        }
                        let _ = hub_log_pump_handle.emit("device-log", &line);
                    }
                }
            });

            // Spawn background push services (Data ~1Hz, RGB ~30fps).
            // setup() is not inside Tokio; use Tauri's runtime, not Handle::current().
            let state_for_bg = app_state.clone();
            tauri::async_runtime::spawn(async move {
                let s = state_for_bg.read().await;
                tray::spawn_background_services(
                    s.device_mgr.clone(),
                    s.data_engine.clone(),
                    s.rgb_engine.clone(),
                );
            });

            // Close button: hide to tray (was prevent_close only — window stayed visible, looked broken on Windows).
            let window = match app.get_webview_window("main") {
                Some(w) => w,
                None => {
                    log::error!("Missing webview window 'main'; check tauri.conf.json (windows[].label)");
                    std::process::exit(1);
                }
            };
            let handle = app.handle().clone();
            window.on_window_event(move |event| {
                if let tauri::WindowEvent::CloseRequested { api, .. } = event {
                    if AppSettings::load_or_default().minimize_to_tray {
                        api.prevent_close();
                        if let Some(w) = handle.get_webview_window("main") {
                            let _ = w.hide();
                        }
                    } else {
                        api.prevent_close();
                        handle.exit(0);
                    }
                }
            });

            Ok(())
        })
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
