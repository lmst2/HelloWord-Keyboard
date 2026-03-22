use crate::eink::EinkPipeline;
use crate::state::SharedState;
use tauri::State;

#[tauri::command]
pub async fn eink_upload_image(
    state: State<'_, SharedState>,
    image_data: Vec<u8>,
    slot: u8,
) -> Result<(), String> {
    log::info!(
        "eink_upload_image: slot={} raw_input_bytes={}",
        slot,
        image_data.len()
    );
    let s = state.inner().read().await;
    let mono = s.eink_pipeline.prepare_image(&image_data)?;
    let pages = EinkPipeline::split_into_pages(&mono);
    log::info!(
        "eink_upload_image: prepared mono_bytes={} page_count={}",
        mono.len(),
        pages.len()
    );

    let mut dm = s.device_mgr.lock().await;
    for (idx, (page_num, page_data)) in pages.iter().enumerate() {
        log::debug!(
            "eink_upload_image: sending chunk {}/{} protocol_page={} ({} bytes) slot={}",
            idx + 1,
            pages.len(),
            page_num,
            page_data.len(),
            slot
        );
        dm.hub_eink_upload(slot, *page_num, page_data)?;
        // Small delay between pages to avoid overflowing device buffer
        tokio::time::sleep(std::time::Duration::from_millis(5)).await;
    }
    log::info!(
        "eink_upload_image: done slot={} total_pages={}",
        slot,
        pages.len()
    );
    Ok(())
}

#[tauri::command]
pub async fn eink_send_text(state: State<'_, SharedState>, text: String) -> Result<(), String> {
    log::info!(
        "eink_send_text: len={} chars_preview={:?}",
        text.len(),
        text.chars().take(48).collect::<String>()
    );
    let s = state.inner().read().await;
    let mut dm = s.device_mgr.lock().await;
    dm.hub_eink_send_text(&text)?;
    log::info!(
        "eink_send_text: CDC write ok (no HUB ack in protocol; check device logs / e-ink)"
    );
    Ok(())
}

#[tauri::command]
pub async fn eink_switch_app(state: State<'_, SharedState>, app_id: u8) -> Result<(), String> {
    log::info!("eink_switch_app: app_id=0x{:02X}", app_id);
    let s = state.inner().read().await;
    let mut dm = s.device_mgr.lock().await;
    dm.hub_switch_eink_app(app_id)?;
    log::info!(
        "eink_switch_app: hub ack ok app_id=0x{:02X}",
        app_id
    );
    Ok(())
}
