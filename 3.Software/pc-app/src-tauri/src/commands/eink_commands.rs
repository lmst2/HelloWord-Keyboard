use crate::eink::EinkPipeline;
use crate::state::SharedState;
use tauri::State;

#[tauri::command]
pub async fn eink_upload_image(
    state: State<'_, SharedState>,
    image_data: Vec<u8>,
    slot: u8,
) -> Result<(), String> {
    let s = state.inner().read().await;
    let mono = s.eink_pipeline.prepare_image(&image_data)?;
    let pages = EinkPipeline::split_into_pages(&mono);

    let mut dm = s.device_mgr.lock().await;
    for (page_num, page_data) in pages {
        dm.hub_eink_upload(slot, page_num, &page_data)?;
        // Small delay between pages to avoid overflowing device buffer
        tokio::time::sleep(std::time::Duration::from_millis(5)).await;
    }
    Ok(())
}

#[tauri::command]
pub async fn eink_send_text(state: State<'_, SharedState>, text: String) -> Result<(), String> {
    let s = state.inner().read().await;
    let mut dm = s.device_mgr.lock().await;
    dm.hub_eink_send_text(&text)
}

#[tauri::command]
pub async fn eink_switch_app(state: State<'_, SharedState>, app_id: u8) -> Result<(), String> {
    let s = state.inner().read().await;
    let mut dm = s.device_mgr.lock().await;
    dm.hub_switch_eink_app(app_id)
}
