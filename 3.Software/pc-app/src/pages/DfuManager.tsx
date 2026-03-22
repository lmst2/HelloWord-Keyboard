import { useEffect, useRef, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import {
  FirmwareInfo,
  dfuGetInfo,
} from "@/lib/tauri-commands";

interface FlashProgress {
  progress: number;
  message: string;
  done: boolean;
  error: string | null;
}

export default function DfuManager() {
  const [info, setInfo] = useState<FirmwareInfo | null>(null);
  const [loading, setLoading] = useState(false);
  const [flashTarget, setFlashTarget] = useState<"keyboard" | "hub" | null>(null);
  const [progress, setProgress] = useState<FlashProgress | null>(null);
  const [kbFirmware, setKbFirmware] = useState<Uint8Array | null>(null);
  const [hubFirmware, setHubFirmware] = useState<Uint8Array | null>(null);
  const [kbFileName, setKbFileName] = useState<string>("");
  const [hubFileName, setHubFileName] = useState<string>("");
  const kbFileRef = useRef<HTMLInputElement>(null);
  const hubFileRef = useRef<HTMLInputElement>(null);
  const pollRef = useRef<ReturnType<typeof setInterval>>();

  const fetchInfo = async () => {
    setLoading(true);
    try {
      const fw = await dfuGetInfo();
      setInfo(fw);
    } catch {
      // Hub not connected
    } finally {
      setLoading(false);
    }
  };

  const handleFileSelect = async (
    e: React.ChangeEvent<HTMLInputElement>,
    target: "keyboard" | "hub"
  ) => {
    const file = e.target.files?.[0];
    if (!file) return;
    const buf = new Uint8Array(await file.arrayBuffer());
    if (target === "keyboard") {
      setKbFirmware(buf);
      setKbFileName(file.name);
    } else {
      setHubFirmware(buf);
      setHubFileName(file.name);
    }
  };

  const startFlash = async (target: "keyboard" | "hub") => {
    const fw = target === "keyboard" ? kbFirmware : hubFirmware;
    if (!fw) return;

    if (pollRef.current) {
      clearInterval(pollRef.current);
      pollRef.current = undefined;
    }

    setFlashTarget(target);
    setProgress({ progress: 0, message: "Starting...", done: false, error: null });

    // Poll while invoke runs — Rust updates progress during flash; previously we only polled after await, so the bar stayed at 0% then jumped to 100%.
    const pollOnce = async () => {
      try {
        const p = await invoke<FlashProgress>("dfu_get_progress");
        setProgress(p);
        if (p.done && pollRef.current) {
          clearInterval(pollRef.current);
          pollRef.current = undefined;
        }
      } catch {
        // ignore
      }
    };
    void pollOnce();
    pollRef.current = setInterval(pollOnce, 200);

    try {
      const cmd = target === "keyboard" ? "dfu_flash_keyboard" : "dfu_flash_hub";
      await invoke(cmd, { firmwareBytes: Array.from(fw) });
    } catch (err) {
      setProgress((p) =>
        p ? { ...p, error: String(err), done: true } : null
      );
    } finally {
      if (pollRef.current) {
        clearInterval(pollRef.current);
        pollRef.current = undefined;
      }
      try {
        const p = await invoke<FlashProgress>("dfu_get_progress");
        setProgress(p);
      } catch {
        // ignore
      }
    }
  };

  useEffect(() => {
    return () => {
      if (pollRef.current) clearInterval(pollRef.current);
    };
  }, []);

  const ProgressBar = ({ value }: { value: number }) => (
    <div className="w-full bg-hw-bg rounded-full h-3 overflow-hidden">
      <div
        className="bg-hw-accent h-full rounded-full transition-all duration-300"
        style={{ width: `${Math.min(100, value * 100)}%` }}
      />
    </div>
  );

  return (
    <div className="p-6 overflow-y-auto h-full space-y-6">
      <h2 className="text-lg font-semibold">Firmware Update</h2>

      {/* Firmware Info */}
      <div className="bg-hw-surface border border-hw-border rounded-lg p-4">
        <div className="flex items-center justify-between mb-3">
          <h3 className="text-sm font-medium text-hw-text-dim">Current Firmware</h3>
          <button
            onClick={fetchInfo}
            disabled={loading}
            className="px-3 py-1 bg-hw-accent/20 text-hw-accent-light rounded text-xs hover:bg-hw-accent/30"
          >
            {loading ? "..." : "Refresh"}
          </button>
        </div>
        {info ? (
          <div className="grid grid-cols-2 gap-3">
            <div className="bg-hw-bg rounded p-3">
              <p className="text-xs text-hw-text-dim">Keyboard</p>
              <p className="text-lg font-bold">{info.kb_version ?? "Unknown"}</p>
            </div>
            <div className="bg-hw-bg rounded p-3">
              <p className="text-xs text-hw-text-dim">Hub</p>
              <p className="text-lg font-bold">{info.hub_version ?? "Unknown"}</p>
            </div>
          </div>
        ) : (
          <p className="text-sm text-hw-text-dim">Connect Hub and click Refresh.</p>
        )}
      </div>

      {/* Flash Cards */}
      <div className="grid grid-cols-2 gap-4">
        {/* Keyboard Flash */}
        <div className="bg-hw-surface border border-hw-border rounded-lg p-4 space-y-3">
          <h3 className="text-sm font-medium">Keyboard Firmware</h3>
          <p className="text-xs text-hw-text-dim">
            Select .bin file. The app will auto-reboot keyboard to bootloader,
            flash, verify CRC, and reboot.
          </p>
          <input
            ref={kbFileRef}
            type="file"
            accept=".bin"
            onChange={(e) => handleFileSelect(e, "keyboard")}
            className="hidden"
          />
          <button
            onClick={() => kbFileRef.current?.click()}
            className="w-full px-4 py-2 bg-hw-bg border border-hw-border rounded text-sm hover:bg-white/5"
          >
            {kbFileName || "Choose .bin file"}
          </button>
          <button
            onClick={() => startFlash("keyboard")}
            disabled={!kbFirmware || flashTarget !== null}
            className="w-full px-4 py-2 bg-hw-warning text-black rounded text-sm font-medium hover:bg-hw-warning/80 disabled:opacity-50"
          >
            Flash Keyboard
          </button>
        </div>

        {/* Hub Flash */}
        <div className="bg-hw-surface border border-hw-border rounded-lg p-4 space-y-3">
          <h3 className="text-sm font-medium">Hub Firmware</h3>
          <p className="text-xs text-hw-text-dim">
            Select .bin file. The Hub will enter STM32 DFU mode for flashing.
          </p>
          <input
            ref={hubFileRef}
            type="file"
            accept=".bin"
            onChange={(e) => handleFileSelect(e, "hub")}
            className="hidden"
          />
          <button
            onClick={() => hubFileRef.current?.click()}
            className="w-full px-4 py-2 bg-hw-bg border border-hw-border rounded text-sm hover:bg-white/5"
          >
            {hubFileName || "Choose .bin file"}
          </button>
          <button
            onClick={() => startFlash("hub")}
            disabled={!hubFirmware || flashTarget !== null}
            className="w-full px-4 py-2 bg-hw-warning text-black rounded text-sm font-medium hover:bg-hw-warning/80 disabled:opacity-50"
          >
            Flash Hub
          </button>
        </div>
      </div>

      {/* Progress */}
      {progress && (
        <div className="bg-hw-surface border border-hw-border rounded-lg p-4 space-y-3">
          <h3 className="text-sm font-medium">
            Flashing {flashTarget === "keyboard" ? "Keyboard" : "Hub"}...
          </h3>
          <ProgressBar value={progress.progress} />
          <p className="text-sm">{progress.message}</p>
          {progress.error && (
            <p className="text-sm text-hw-error">{progress.error}</p>
          )}
          {progress.done && !progress.error && (
            <p className="text-sm text-hw-success font-medium">
              Flash completed successfully!
            </p>
          )}
          {progress.done && (
            <button
              onClick={() => {
                setProgress(null);
                setFlashTarget(null);
              }}
              className="px-4 py-2 bg-hw-accent text-white rounded text-sm"
            >
              Done
            </button>
          )}
        </div>
      )}
    </div>
  );
}
