import { useState } from "react";
import { useConfig } from "@/hooks/useConfig";
import ParamEditor from "@/components/config/ParamEditor";

export default function ConfigEditor() {
  const { registry, values, categories, writeParam } = useConfig();
  const [expandedCat, setExpandedCat] = useState<string | null>(
    categories[0] || null
  );

  const toggleCat = (cat: string) =>
    setExpandedCat((prev) => (prev === cat ? null : cat));

  return (
    <div className="p-6 overflow-y-auto h-full">
      <h2 className="text-lg font-semibold mb-4">Configuration</h2>

      <div className="space-y-2">
        {categories.map((cat) => {
          const params = registry.filter((p) => p.category === cat);
          const isOpen = expandedCat === cat;

          return (
            <div
              key={cat}
              className="bg-hw-surface border border-hw-border rounded-lg overflow-hidden"
            >
              <button
                onClick={() => toggleCat(cat)}
                className="w-full flex items-center justify-between px-4 py-3 text-sm font-medium hover:bg-white/5 transition-colors"
              >
                <span>{cat}</span>
                <span className="text-hw-text-dim text-xs">
                  {params.length} params {isOpen ? "▲" : "▼"}
                </span>
              </button>
              {isOpen && (
                <div className="px-4 pb-3 border-t border-hw-border">
                  {params.map((p) => {
                    const raw = values[p.id];
                    const val = raw ? raw[0] : p.default_value[0] ?? 0;
                    const target =
                      p.target === "Keyboard" ? "kb" : "hub";
                    return (
                      <ParamEditor
                        key={p.id}
                        param={p}
                        value={val}
                        onChange={(v) =>
                          writeParam(
                            target,
                            p.id,
                            v as number
                          )
                        }
                      />
                    );
                  })}
                </div>
              )}
            </div>
          );
        })}
      </div>
    </div>
  );
}
