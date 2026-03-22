use crate::config::ConfigService;
use crate::data::DataProviderEngine;
use crate::device::DeviceManager;
use crate::dfu::DfuService;
use crate::eink::EinkPipeline;
use crate::profile::ProfileService;
use crate::rgb::RgbEngine;
use crate::settings::AppSettings;
use std::sync::Arc;
use tokio::sync::RwLock;

pub type SharedState = Arc<RwLock<AppState>>;

pub struct AppState {
    pub device_mgr: Arc<RwLock<DeviceManager>>,
    pub config_svc: Arc<RwLock<ConfigService>>,
    pub data_engine: Arc<RwLock<DataProviderEngine>>,
    pub rgb_engine: Arc<RwLock<RgbEngine>>,
    pub eink_pipeline: Arc<EinkPipeline>,
    pub profile_svc: Arc<ProfileService>,
    pub dfu_svc: Arc<DfuService>,
    pub settings: Arc<RwLock<AppSettings>>,
}

impl AppState {
    pub fn new() -> Self {
        let device_mgr = Arc::new(RwLock::new(DeviceManager::new()));
        let config_svc = Arc::new(RwLock::new(ConfigService::new()));
        let data_engine = Arc::new(RwLock::new(DataProviderEngine::new()));
        let rgb_engine = Arc::new(RwLock::new(RgbEngine::new()));
        let eink_pipeline = Arc::new(EinkPipeline::new());
        let profile_svc = Arc::new(ProfileService::new());
        let dfu_svc = Arc::new(DfuService::new());
        let settings = Arc::new(RwLock::new(AppSettings::load_or_default()));

        Self {
            device_mgr,
            config_svc,
            data_engine,
            rgb_engine,
            eink_pipeline,
            profile_svc,
            dfu_svc,
            settings,
        }
    }
}
