import { useEffect, useState } from "react";
import { AppSettings, settingsGet, settingsSet } from "@/lib/tauri-commands";

export default function AppSettingsPage() {
  const [settings, setSettings] = useState<AppSettings | null>(null);
  const [saved, setSaved] = useState(false);

  useEffect(() => {
    settingsGet()
      .then(setSettings)
      .catch(() => {});
  }, []);

  const update = (patch: Partial<AppSettings>) => {
    if (!settings) return;
    setSettings({ ...settings, ...patch });
    setSaved(false);
  };

  const save = async () => {
    if (!settings) return;
    try {
      await settingsSet(settings);
      setSaved(true);
      setTimeout(() => setSaved(false), 2000);
    } catch (e) {
      console.error("Save failed:", e);
    }
  };

  if (!settings) {
    return (
      <div className="p-6 text-hw-text-dim">Loading settings...</div>
    );
  }

  return (
    <div className="p-6 overflow-y-auto h-full space-y-6">
      <h2 className="text-lg font-semibold">App Settings</h2>

      <div className="bg-hw-surface border border-hw-border rounded-lg p-4 space-y-4">
        {/* Minimize to Tray */}
        <div className="flex items-center justify-between">
          <span className="text-sm">Minimize to System Tray</span>
          <button
            onClick={() => update({ minimize_to_tray: !settings.minimize_to_tray })}
            className={`w-10 h-5 rounded-full transition-colors relative ${
              settings.minimize_to_tray ? "bg-hw-accent" : "bg-hw-border"
            }`}
          >
            <span
              className={`absolute top-0.5 w-4 h-4 rounded-full bg-white transition-transform ${
                settings.minimize_to_tray ? "left-5" : "left-0.5"
              }`}
            />
          </button>
        </div>

        {/* Auto Start */}
        <div className="flex items-center justify-between">
          <span className="text-sm">Auto Start on Login</span>
          <button
            onClick={() => update({ auto_start: !settings.auto_start })}
            className={`w-10 h-5 rounded-full transition-colors relative ${
              settings.auto_start ? "bg-hw-accent" : "bg-hw-border"
            }`}
          >
            <span
              className={`absolute top-0.5 w-4 h-4 rounded-full bg-white transition-transform ${
                settings.auto_start ? "left-5" : "left-0.5"
              }`}
            />
          </button>
        </div>

        {/* Auto Detect OS */}
        <div className="flex items-center justify-between">
          <span className="text-sm">Auto Detect OS Mode</span>
          <button
            onClick={() => update({ auto_detect_os: !settings.auto_detect_os })}
            className={`w-10 h-5 rounded-full transition-colors relative ${
              settings.auto_detect_os ? "bg-hw-accent" : "bg-hw-border"
            }`}
          >
            <span
              className={`absolute top-0.5 w-4 h-4 rounded-full bg-white transition-transform ${
                settings.auto_detect_os ? "left-5" : "left-0.5"
              }`}
            />
          </button>
        </div>

        {/* Theme */}
        <div className="flex items-center justify-between">
          <span className="text-sm">Theme</span>
          <select
            value={settings.theme}
            onChange={(e) => update({ theme: e.target.value })}
            className="bg-hw-bg border border-hw-border rounded px-2 py-1 text-sm"
          >
            <option value="dark">Dark</option>
            <option value="light">Light</option>
            <option value="system">System</option>
          </select>
        </div>
      </div>

      {/* OpenRGB Integration */}
      <div className="bg-hw-surface border border-hw-border rounded-lg p-4 space-y-3">
        <h3 className="text-sm font-medium text-hw-text-dim">
          OpenRGB Integration (Phase F)
        </h3>
        <div className="flex items-center justify-between">
          <span className="text-sm">Enable OpenRGB Sync</span>
          <button
            onClick={() => update({ openrgb_enabled: !settings.openrgb_enabled })}
            className={`w-10 h-5 rounded-full transition-colors relative ${
              settings.openrgb_enabled ? "bg-hw-accent" : "bg-hw-border"
            }`}
          >
            <span
              className={`absolute top-0.5 w-4 h-4 rounded-full bg-white transition-transform ${
                settings.openrgb_enabled ? "left-5" : "left-0.5"
              }`}
            />
          </button>
        </div>
        <div className="grid grid-cols-2 gap-3">
          <div>
            <label className="text-xs text-hw-text-dim">Host</label>
            <input
              value={settings.openrgb_host}
              onChange={(e) => update({ openrgb_host: e.target.value })}
              className="w-full bg-hw-bg border border-hw-border rounded px-2 py-1 text-sm mt-1"
            />
          </div>
          <div>
            <label className="text-xs text-hw-text-dim">Port</label>
            <input
              type="number"
              value={settings.openrgb_port}
              onChange={(e) => update({ openrgb_port: Number(e.target.value) })}
              className="w-full bg-hw-bg border border-hw-border rounded px-2 py-1 text-sm mt-1"
            />
          </div>
        </div>
      </div>

      {/* Weather */}
      <div className="bg-hw-surface border border-hw-border rounded-lg p-4 space-y-3">
        <h3 className="text-sm font-medium text-hw-text-dim">
          Weather Provider (Phase F)
        </h3>
        <div>
          <label className="text-xs text-hw-text-dim">City</label>
          <input
            value={settings.weather_city}
            onChange={(e) => update({ weather_city: e.target.value })}
            placeholder="e.g. Beijing"
            className="w-full bg-hw-bg border border-hw-border rounded px-2 py-1 text-sm mt-1"
          />
        </div>
        <div>
          <label className="text-xs text-hw-text-dim">API Key (OpenWeatherMap)</label>
          <input
            type="password"
            value={settings.weather_api_key}
            onChange={(e) => update({ weather_api_key: e.target.value })}
            className="w-full bg-hw-bg border border-hw-border rounded px-2 py-1 text-sm mt-1"
          />
        </div>
      </div>

      {/* Device IDs */}
      <div className="bg-hw-surface border border-hw-border rounded-lg p-4 space-y-3">
        <h3 className="text-sm font-medium text-hw-text-dim">
          Device USB IDs
        </h3>
        <div className="grid grid-cols-2 gap-3">
          <div>
            <label className="text-xs text-hw-text-dim">Keyboard VID</label>
            <input
              value={`0x${settings.hid_vid.toString(16).toUpperCase()}`}
              readOnly
              className="w-full bg-hw-bg border border-hw-border rounded px-2 py-1 text-sm mt-1 font-mono"
            />
          </div>
          <div>
            <label className="text-xs text-hw-text-dim">Keyboard PID</label>
            <input
              value={`0x${settings.hid_pid.toString(16).toUpperCase()}`}
              readOnly
              className="w-full bg-hw-bg border border-hw-border rounded px-2 py-1 text-sm mt-1 font-mono"
            />
          </div>
          <div>
            <label className="text-xs text-hw-text-dim">Hub VID</label>
            <input
              value={`0x${settings.cdc_vid.toString(16).toUpperCase()}`}
              readOnly
              className="w-full bg-hw-bg border border-hw-border rounded px-2 py-1 text-sm mt-1 font-mono"
            />
          </div>
          <div>
            <label className="text-xs text-hw-text-dim">Hub PID</label>
            <input
              value={`0x${settings.cdc_pid.toString(16).toUpperCase()}`}
              readOnly
              className="w-full bg-hw-bg border border-hw-border rounded px-2 py-1 text-sm mt-1 font-mono"
            />
          </div>
        </div>
      </div>

      <button
        onClick={save}
        className="px-6 py-2 bg-hw-accent text-white rounded text-sm font-medium hover:bg-hw-accent-light transition-colors"
      >
        {saved ? "Saved!" : "Save Settings"}
      </button>
    </div>
  );
}
