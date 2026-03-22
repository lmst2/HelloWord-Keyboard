import { invoke } from "@tauri-apps/api/core";

// Device
export interface DeviceStatus {
  keyboard: "Connected" | "Disconnected";
  hub: "Connected" | "Disconnected";
  kb_firmware_version: string | null;
  hub_firmware_version: string | null;
}

export const getDeviceStatus = () =>
  invoke<DeviceStatus>("get_device_status");
export const startDiscovery = () => invoke<void>("start_discovery");
export const stopDiscovery = () => invoke<void>("stop_discovery");

// Config
export interface ParamMeta {
  id: number;
  name: string;
  category: string;
  target: "Keyboard" | "Hub";
  value_type: "U8" | "U16" | "Bool" | "Enum";
  min: number;
  max: number;
  step: number;
  enum_labels: string[] | null;
  default_value: number[];
}

export interface ConfigValue {
  param: number;
  value: number[];
}

export const configGet = (target: string, param: number) =>
  invoke<ConfigValue>("config_get", { args: { target, param } });

export const configSet = (
  target: string,
  param: number,
  value: number | boolean
) => invoke<void>("config_set", { args: { target, param, value } });

export const configSyncAll = () => invoke<void>("config_sync_all");

export const getParamRegistry = () =>
  invoke<ParamMeta[]>("get_param_registry");

// RGB
export interface RgbModeInfo {
  mode: string;
  running: boolean;
}

export const rgbSetMode = (mode: object) =>
  invoke<void>("rgb_set_mode", { mode });
export const rgbGetMode = () => invoke<RgbModeInfo>("rgb_get_mode");
export const rgbStop = () => invoke<void>("rgb_stop");

// Data
export interface FeedData {
  feed_id: number;
  value_f32: number;
  display: string;
}

export interface ProviderInfo {
  id: string;
  name: string;
  enabled: boolean;
  interval_ms: number;
  feeds: { feed_id: number; name: string; unit: string }[];
}

export const dataStart = () => invoke<void>("data_start");
export const dataStop = () => invoke<void>("data_stop");
export const dataGetProviders = () =>
  invoke<ProviderInfo[]>("data_get_providers");
export const dataSetProviderEnabled = (id: string, enabled: boolean) =>
  invoke<void>("data_set_provider_enabled", { id, enabled });
export const dataGetLive = () => invoke<FeedData[]>("data_get_live");

// E-Ink
export const einkUploadImage = (imageData: number[], slot: number) =>
  invoke<void>("eink_upload_image", { imageData, slot });
export const einkSendText = (text: string) =>
  invoke<void>("eink_send_text", { text });
export const einkSwitchApp = (appId: number) =>
  invoke<void>("eink_switch_app", { appId });

// Profiles
export interface ProfileEntry {
  slot: number;
  name: string;
  used: boolean;
}

export const profileList = () => invoke<ProfileEntry[]>("profile_list");
export const profileSave = (slot: number, name: string) =>
  invoke<void>("profile_save", { slot, name });
export const profileLoad = (slot: number) =>
  invoke<void>("profile_load", { slot });
export const profileDelete = (slot: number) =>
  invoke<void>("profile_delete", { slot });
export const profileExport = (name: string, path: string) =>
  invoke<void>("profile_export", { name, path });
export const profileImport = (path: string) =>
  invoke<string>("profile_import", { path });

// DFU
export interface FirmwareInfo {
  kb_version: string | null;
  hub_version: string | null;
  build_time: string | null;
}

export const dfuGetInfo = () => invoke<FirmwareInfo>("dfu_get_info");
export const dfuFlashKeyboard = (firmwareBytes: number[]) =>
  invoke<void>("dfu_flash_keyboard", { firmwareBytes });
export const dfuFlashHub = (firmwareBytes: number[]) =>
  invoke<void>("dfu_flash_hub", { firmwareBytes });
export const dfuGetProgress = () =>
  invoke<{ progress: number; message: string; done: boolean; error: string | null }>(
    "dfu_get_progress"
  );

// Settings
export interface AppSettings {
  minimize_to_tray: boolean;
  auto_start: boolean;
  theme: string;
  data_providers_enabled: string[];
  auto_detect_os: boolean;
  hid_vid: number;
  hid_pid: number;
  cdc_vid: number;
  cdc_pid: number;
  openrgb_enabled: boolean;
  openrgb_host: string;
  openrgb_port: number;
  weather_city: string;
  weather_api_key: string;
  device_log_enabled: boolean;
  device_log_max_level: number;
  pc_app_log_level: string;
}

export interface DeviceLogLine {
  ts_ms: number;
  source: string;
  level: string;
  message: string;
}

export const settingsGet = () => invoke<AppSettings>("settings_get");
export const settingsSet = (settings: AppSettings) =>
  invoke<void>("settings_set", { newSettings: settings });

export const logGetSnapshot = () =>
  invoke<DeviceLogLine[]>("log_get_snapshot");
export const logClear = () => invoke<void>("log_clear");
export const logSyncDevice = () => invoke<void>("log_sync_device");
