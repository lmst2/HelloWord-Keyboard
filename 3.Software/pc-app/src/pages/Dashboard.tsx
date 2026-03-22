import { useEffect, useState } from "react";
import { useDeviceStatus } from "@/hooks/useDeviceStatus";
import { useDataFeed } from "@/hooks/useDataFeed";
import {
  configGet,
  configSet,
} from "@/lib/tauri-commands";
import { ConfigParam, EFFECT_NAMES } from "@/lib/config-params";

export default function Dashboard() {
  const status = useDeviceStatus();
  const { feeds, running, start, stop } = useDataFeed();
  const [brightness, setBrightness] = useState(4);
  const [effectIdx, setEffectIdx] = useState(0);

  useEffect(() => {
    if (status.keyboard === "Connected") {
      configGet("kb", ConfigParam.PARAM_BRIGHTNESS).then((v) => {
        if (v?.value?.[0] !== undefined) setBrightness(v.value[0]);
      }).catch(() => {});
      configGet("kb", ConfigParam.PARAM_EFFECT_MODE).then((v) => {
        if (v?.value?.[0] !== undefined) setEffectIdx(v.value[0]);
      }).catch(() => {});
    }
  }, [status.keyboard]);

  const DeviceCard = ({
    title,
    connected,
    version,
  }: {
    title: string;
    connected: boolean;
    version: string | null;
  }) => (
    <div
      className={`rounded-lg border p-4 ${
        connected
          ? "border-hw-success/30 bg-hw-success/5"
          : "border-hw-border bg-hw-surface"
      }`}
    >
      <div className="flex items-center gap-2 mb-2">
        <span
          className={`w-2.5 h-2.5 rounded-full ${
            connected ? "bg-hw-success" : "bg-hw-error"
          }`}
        />
        <span className="font-medium text-sm">{title}</span>
      </div>
      <p className="text-xs text-hw-text-dim">
        {connected ? "Connected" : "Disconnected"}
      </p>
      {version && (
        <p className="text-xs text-hw-text-dim mt-1">FW: {version}</p>
      )}
    </div>
  );

  return (
    <div className="p-6 space-y-6 overflow-y-auto h-full">
      <h2 className="text-lg font-semibold">Dashboard</h2>

      {/* Device Status */}
      <div className="grid grid-cols-2 gap-4">
        <DeviceCard
          title="Keyboard"
          connected={status.keyboard === "Connected"}
          version={status.kb_firmware_version}
        />
        <DeviceCard
          title="Hub"
          connected={status.hub === "Connected"}
          version={status.hub_firmware_version}
        />
      </div>

      {/* Quick Controls */}
      <div className="bg-hw-surface rounded-lg border border-hw-border p-4 space-y-4">
        <h3 className="text-sm font-medium text-hw-text-dim">
          Quick Controls
        </h3>

        <div>
          <div className="flex items-center justify-between mb-1">
            <span className="text-sm">Brightness</span>
            <span className="text-xs text-hw-text-dim tabular-nums">
              {brightness}
            </span>
          </div>
          <input
            type="range"
            min={0}
            max={6}
            value={brightness}
            onChange={(e) => {
              const v = Number(e.target.value);
              setBrightness(v);
              configSet("kb", ConfigParam.PARAM_BRIGHTNESS, v).catch(() => {});
            }}
            className="w-full accent-hw-accent"
          />
        </div>

        <div className="flex items-center justify-between">
          <span className="text-sm">Effect</span>
          <select
            value={effectIdx}
            onChange={(e) => {
              const v = Number(e.target.value);
              setEffectIdx(v);
              configSet("kb", ConfigParam.PARAM_EFFECT_MODE, v).catch(() => {});
            }}
            className="bg-hw-bg border border-hw-border rounded px-2 py-1 text-sm"
          >
            {EFFECT_NAMES.map((name, i) => (
              <option key={i} value={i}>
                {name}
              </option>
            ))}
          </select>
        </div>
      </div>

      {/* Live Data */}
      <div className="bg-hw-surface rounded-lg border border-hw-border p-4">
        <div className="flex items-center justify-between mb-3">
          <h3 className="text-sm font-medium text-hw-text-dim">
            System Stats
          </h3>
          <button
            onClick={running ? stop : start}
            className={`text-xs px-3 py-1 rounded ${
              running
                ? "bg-hw-error/20 text-hw-error"
                : "bg-hw-accent/20 text-hw-accent-light"
            }`}
          >
            {running ? "Stop" : "Start"}
          </button>
        </div>
        {feeds.length === 0 ? (
          <p className="text-xs text-hw-text-dim">
            {running ? "Waiting for data..." : "Click Start to begin monitoring"}
          </p>
        ) : (
          <div className="grid grid-cols-2 gap-3">
            {feeds.map((f) => (
              <div key={f.feed_id} className="bg-hw-bg rounded p-3">
                <p className="text-2xl font-bold tabular-nums">
                  {f.display}
                </p>
                <p className="text-xs text-hw-text-dim mt-1">
                  Feed 0x{f.feed_id.toString(16).padStart(2, "0")}
                </p>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
}
