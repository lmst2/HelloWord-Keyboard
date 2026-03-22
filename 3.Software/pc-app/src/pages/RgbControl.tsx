import { useEffect, useState } from "react";
import { rgbSetMode, rgbGetMode, rgbStop } from "@/lib/tauri-commands";
import KeyboardVisualizer from "@/components/keyboard/KeyboardVisualizer";

type ModeType = "off" | "cpu_temp" | "static";

export default function RgbControl() {
  const [mode, setMode] = useState<ModeType>("off");
  const [running, setRunning] = useState(false);
  const [color, setColor] = useState({ r: 128, g: 0, b: 255 });
  const [coolColor, setCoolColor] = useState({ r: 0, g: 100, b: 255 });
  const [hotColor, setHotColor] = useState({ r: 255, g: 30, b: 0 });
  const [tempRange, setTempRange] = useState({ min: 30, max: 90 });

  useEffect(() => {
    rgbGetMode()
      .then((info) => {
        setMode(info.mode as ModeType);
        setRunning(info.running);
      })
      .catch(() => {});
  }, []);

  const applyMode = async () => {
    try {
      if (mode === "off") {
        await rgbStop();
        setRunning(false);
        return;
      }
      if (mode === "cpu_temp") {
        await rgbSetMode({
          CpuTempGradient: {
            cool_color: coolColor,
            hot_color: hotColor,
            temp_range: [tempRange.min, tempRange.max],
          },
        });
      } else {
        await rgbSetMode({ StaticColor: color });
      }
      setRunning(true);
    } catch (e) {
      console.error("RGB mode error:", e);
    }
  };

  const ColorInput = ({
    label,
    value,
    onChange,
  }: {
    label: string;
    value: { r: number; g: number; b: number };
    onChange: (c: { r: number; g: number; b: number }) => void;
  }) => {
    const hex = `#${value.r.toString(16).padStart(2, "0")}${value.g.toString(16).padStart(2, "0")}${value.b.toString(16).padStart(2, "0")}`;
    return (
      <div className="flex items-center gap-3">
        <span className="text-sm w-24">{label}</span>
        <input
          type="color"
          value={hex}
          onChange={(e) => {
            const h = e.target.value;
            onChange({
              r: parseInt(h.slice(1, 3), 16),
              g: parseInt(h.slice(3, 5), 16),
              b: parseInt(h.slice(5, 7), 16),
            });
          }}
          className="w-10 h-8 rounded border-0 cursor-pointer"
        />
        <span className="text-xs text-hw-text-dim font-mono">{hex}</span>
      </div>
    );
  };

  return (
    <div className="p-6 overflow-y-auto h-full space-y-6">
      <h2 className="text-lg font-semibold">RGB Control</h2>

      <div className="bg-hw-surface border border-hw-border rounded-lg p-4 space-y-4">
        <div className="flex items-center gap-2">
          <span className="text-sm w-16">Mode</span>
          <select
            value={mode}
            onChange={(e) => setMode(e.target.value as ModeType)}
            className="bg-hw-bg border border-hw-border rounded px-3 py-1.5 text-sm flex-1"
          >
            <option value="off">Off (firmware control)</option>
            <option value="cpu_temp">CPU Temperature Gradient</option>
            <option value="static">Static Color</option>
          </select>
        </div>

        {mode === "cpu_temp" && (
          <div className="space-y-3 pl-4 border-l-2 border-hw-accent/30">
            <ColorInput
              label="Cool Color"
              value={coolColor}
              onChange={setCoolColor}
            />
            <ColorInput
              label="Hot Color"
              value={hotColor}
              onChange={setHotColor}
            />
            <div className="flex items-center gap-3">
              <span className="text-sm w-24">Temp Range</span>
              <input
                type="number"
                value={tempRange.min}
                onChange={(e) =>
                  setTempRange((p) => ({ ...p, min: Number(e.target.value) }))
                }
                className="w-16 bg-hw-bg border border-hw-border rounded px-2 py-1 text-sm"
              />
              <span className="text-hw-text-dim">—</span>
              <input
                type="number"
                value={tempRange.max}
                onChange={(e) =>
                  setTempRange((p) => ({ ...p, max: Number(e.target.value) }))
                }
                className="w-16 bg-hw-bg border border-hw-border rounded px-2 py-1 text-sm"
              />
              <span className="text-xs text-hw-text-dim">°C</span>
            </div>
          </div>
        )}

        {mode === "static" && (
          <div className="pl-4 border-l-2 border-hw-accent/30">
            <ColorInput label="Color" value={color} onChange={setColor} />
          </div>
        )}

        <div className="flex items-center gap-3 pt-2">
          <button
            onClick={applyMode}
            className="px-4 py-2 bg-hw-accent text-white rounded text-sm font-medium hover:bg-hw-accent-light transition-colors"
          >
            Apply
          </button>
          {running && (
            <span className="text-xs text-hw-success flex items-center gap-1">
              <span className="w-1.5 h-1.5 rounded-full bg-hw-success animate-pulse" />
              Running
            </span>
          )}
        </div>
      </div>

      {/* Keyboard preview */}
      <div className="bg-hw-surface border border-hw-border rounded-lg p-4">
        <h3 className="text-sm font-medium text-hw-text-dim mb-3">
          Keyboard Preview
        </h3>
        <div className="bg-hw-bg rounded p-4 flex justify-center">
          <KeyboardVisualizer
            activeColor={
              mode === "static"
                ? `rgb(${color.r},${color.g},${color.b})`
                : mode === "cpu_temp"
                  ? `rgb(${coolColor.r},${coolColor.g},${coolColor.b})`
                  : "#6366f1"
            }
          />
        </div>
      </div>
    </div>
  );
}
