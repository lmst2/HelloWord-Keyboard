import { useCallback, useEffect, useRef, useState } from "react";
import { listen, type UnlistenFn } from "@tauri-apps/api/event";
import {
  logClear,
  logGetSnapshot,
  logSyncDevice,
  settingsGet,
  settingsSet,
  type AppSettings,
  type DeviceLogLine,
} from "@/lib/tauri-commands";

const MAX_LINES_UI = 1500;

function formatTs(ms: number): string {
  if (!ms) return "—";
  const d = new Date(Number(ms));
  return d.toLocaleTimeString(undefined, {
    hour12: false,
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
  });
}

export default function Logs() {
  const [lines, setLines] = useState<DeviceLogLine[]>([]);
  const [settings, setSettings] = useState<AppSettings | null>(null);
  const bottomRef = useRef<HTMLDivElement>(null);
  const [autoScroll, setAutoScroll] = useState(true);

  const loadSnapshot = useCallback(async () => {
    try {
      const snap = await logGetSnapshot();
      setLines(snap.slice(-MAX_LINES_UI));
    } catch {
      /* ignore */
    }
  }, []);

  const loadSettings = useCallback(async () => {
    try {
      const s = await settingsGet();
      setSettings(s);
    } catch {
      /* ignore */
    }
  }, []);

  useEffect(() => {
    loadSnapshot();
    loadSettings();
  }, [loadSnapshot, loadSettings]);

  useEffect(() => {
    let unlisten: UnlistenFn | undefined;
    const p = listen<DeviceLogLine>("device-log", (ev: { payload: DeviceLogLine }) => {
      setLines((prev: DeviceLogLine[]) => {
        const next = [...prev, ev.payload];
        if (next.length > MAX_LINES_UI) {
          return next.slice(-MAX_LINES_UI);
        }
        return next;
      });
    });
    p.then((fn: UnlistenFn) => {
      unlisten = fn;
    }).catch(() => {});
    return () => {
      unlisten?.();
    };
  }, []);

  useEffect(() => {
    if (autoScroll) bottomRef.current?.scrollIntoView({ behavior: "smooth" });
  }, [lines, autoScroll]);

  const pushSettings = async (patch: Partial<AppSettings>) => {
    if (!settings) return;
    const next = { ...settings, ...patch };
    setSettings(next);
    await settingsSet(next);
    await logSyncDevice();
  };

  if (!settings) {
    return (
      <div className="p-6 text-sm text-hw-text-dim">Loading…</div>
    );
  }

  return (
    <div className="p-6 flex flex-col gap-4 h-full min-h-0">
      <h2 className="text-lg font-semibold shrink-0">Logs</h2>

      <div className="flex flex-wrap gap-4 items-end bg-hw-surface border border-hw-border rounded-lg p-4 shrink-0">
        <label className="flex items-center gap-2 text-sm cursor-pointer">
          <input
            type="checkbox"
            checked={settings.device_log_enabled}
            onChange={(e) =>
              pushSettings({ device_log_enabled: e.target.checked })
            }
          />
          Device log stream (Hub + Keyboard → PC)
        </label>

        <div className="flex flex-col gap-1">
          <span className="text-xs text-hw-text-dim">Device max level</span>
          <select
            className="bg-hw-bg border border-hw-border rounded px-2 py-1 text-sm"
            value={settings.device_log_max_level}
            onChange={(e) =>
              pushSettings({
                device_log_max_level: Number(e.target.value),
              })
            }
          >
            <option value={0}>Error</option>
            <option value={1}>Warning</option>
            <option value={2}>Info</option>
            <option value={3}>Debug</option>
          </select>
        </div>

        <div className="flex flex-col gap-1">
          <span className="text-xs text-hw-text-dim">PC app log level</span>
          <select
            className="bg-hw-bg border border-hw-border rounded px-2 py-1 text-sm"
            value={settings.pc_app_log_level}
            onChange={(e) =>
              pushSettings({ pc_app_log_level: e.target.value })
            }
          >
            <option value="error">error</option>
            <option value="warn">warn</option>
            <option value="info">info</option>
            <option value="debug">debug</option>
            <option value="trace">trace</option>
          </select>
        </div>

        <button
          type="button"
          className="text-sm px-3 py-1 rounded border border-hw-border hover:bg-white/5"
          onClick={() => logSyncDevice()}
        >
          Sync to Hub
        </button>

        <button
          type="button"
          className="text-sm px-3 py-1 rounded border border-hw-border hover:bg-white/5"
          onClick={() => loadSnapshot()}
        >
          Refresh buffer
        </button>

        <button
          type="button"
          className="text-sm px-3 py-1 rounded border border-hw-error/40 text-hw-error hover:bg-hw-error/10"
          onClick={async () => {
            await logClear();
            setLines([]);
          }}
        >
          Clear view
        </button>

        <label className="flex items-center gap-2 text-sm cursor-pointer ml-auto">
          <input
            type="checkbox"
            checked={autoScroll}
            onChange={(e) => setAutoScroll(e.target.checked)}
          />
          Auto-scroll
        </label>
      </div>

      <p className="text-xs text-hw-text-dim shrink-0">
        Host file log:{" "}
        <code className="text-hw-accent-light">
          %LocalAppData%\helloword-manager\app.log
        </code>
        . Set <code className="text-hw-accent-light">RUST_LOG</code> env for
        extra Rust tracing; PC level above also adjusts{" "}
        <code className="text-hw-accent-light">log::max_level</code> at runtime.
      </p>

      <div className="flex-1 min-h-0 border border-hw-border rounded-lg bg-hw-bg overflow-hidden flex flex-col">
        <div className="flex-1 overflow-auto font-mono text-xs p-2 space-y-0.5">
          {lines.length === 0 ? (
            <p className="text-hw-text-dim p-2">
              No device lines yet. Enable device log stream and use Debug level
              to see Hub/Keyboard steps. Firmware must include log build.
            </p>
          ) : (
            lines.map((l: DeviceLogLine, i: number) => (
              <div
                key={`${l.ts_ms}-${i}`}
                className="whitespace-pre-wrap break-all border-b border-hw-border/30 pb-0.5"
              >
                <span className="text-hw-text-dim">{formatTs(l.ts_ms)}</span>{" "}
                <span className="text-hw-accent">{l.source}</span>{" "}
                <span
                  className={
                    l.level === "error"
                      ? "text-hw-error"
                      : l.level === "warn"
                        ? "text-yellow-400"
                        : "text-hw-text-dim"
                  }
                >
                  [{l.level}]
                </span>{" "}
                {l.message}
              </div>
            ))
          )}
          <div ref={bottomRef} />
        </div>
      </div>
    </div>
  );
}
