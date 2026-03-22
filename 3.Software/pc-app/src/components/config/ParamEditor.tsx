import { ParamMeta } from "@/lib/tauri-commands";

interface Props {
  param: ParamMeta;
  value: number;
  onChange: (value: number | boolean) => void;
}

export default function ParamEditor({ param, value, onChange }: Props) {
  if (param.value_type === "Bool") {
    return (
      <div className="flex items-center justify-between py-2">
        <span className="text-sm">{param.name}</span>
        <button
          onClick={() => onChange(!value)}
          className={`w-10 h-5 rounded-full transition-colors ${
            value ? "bg-hw-accent" : "bg-hw-border"
          } relative`}
        >
          <span
            className={`absolute top-0.5 w-4 h-4 rounded-full bg-white transition-transform ${
              value ? "left-5" : "left-0.5"
            }`}
          />
        </button>
      </div>
    );
  }

  if (param.value_type === "Enum" && param.enum_labels) {
    return (
      <div className="flex items-center justify-between py-2">
        <span className="text-sm">{param.name}</span>
        <select
          value={value}
          onChange={(e) => onChange(Number(e.target.value))}
          className="bg-hw-bg border border-hw-border rounded px-2 py-1 text-sm text-hw-text"
        >
          {param.enum_labels.map((label, i) => (
            <option key={i} value={i}>
              {label}
            </option>
          ))}
        </select>
      </div>
    );
  }

  // Numeric: slider + number
  return (
    <div className="py-2">
      <div className="flex items-center justify-between mb-1">
        <span className="text-sm">{param.name}</span>
        <span className="text-xs text-hw-text-dim tabular-nums">{value}</span>
      </div>
      <input
        type="range"
        min={param.min}
        max={param.max}
        step={param.step}
        value={value}
        onChange={(e) => onChange(Number(e.target.value))}
        className="w-full accent-hw-accent"
      />
    </div>
  );
}
