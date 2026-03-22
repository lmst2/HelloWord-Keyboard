use log::LevelFilter;

/// Host-side filter for `log::` output (app.log + stderr). Independent from device UART logging.
pub fn apply_pc_rust_log_level(level: &str) {
    let filter = match level.to_ascii_lowercase().as_str() {
        "error" => LevelFilter::Error,
        "warn" => LevelFilter::Warn,
        "info" => LevelFilter::Info,
        "debug" => LevelFilter::Debug,
        "trace" => LevelFilter::Trace,
        _ => LevelFilter::Info,
    };
    log::set_max_level(filter);
}
