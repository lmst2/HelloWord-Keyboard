import { useEffect, useState } from "react";
import { useDataFeed } from "@/hooks/useDataFeed";
import {
  ProviderInfo,
  dataGetProviders,
  dataSetProviderEnabled,
} from "@/lib/tauri-commands";

export default function DataProviders() {
  const [providers, setProviders] = useState<ProviderInfo[]>([]);
  const { feeds, running, start, stop } = useDataFeed();

  useEffect(() => {
    dataGetProviders()
      .then(setProviders)
      .catch(() => {});
  }, []);

  const toggleProvider = async (id: string, enabled: boolean) => {
    try {
      await dataSetProviderEnabled(id, enabled);
      setProviders((prev) =>
        prev.map((p) => (p.id === id ? { ...p, enabled } : p))
      );
    } catch (e) {
      console.error("Toggle failed:", e);
    }
  };

  return (
    <div className="p-6 overflow-y-auto h-full space-y-6">
      <div className="flex items-center justify-between">
        <h2 className="text-lg font-semibold">Data Feed</h2>
        <button
          onClick={running ? stop : start}
          className={`px-4 py-2 rounded text-sm font-medium ${
            running
              ? "bg-hw-error/20 text-hw-error hover:bg-hw-error/30"
              : "bg-hw-accent text-white hover:bg-hw-accent-light"
          }`}
        >
          {running ? "Stop Pushing" : "Start Pushing"}
        </button>
      </div>

      {/* Providers */}
      <div className="space-y-3">
        {providers.map((prov) => (
          <div
            key={prov.id}
            className="bg-hw-surface border border-hw-border rounded-lg p-4"
          >
            <div className="flex items-center justify-between mb-3">
              <div>
                <span className="text-sm font-medium">{prov.name}</span>
                <span className="text-xs text-hw-text-dim ml-2">
                  every {prov.interval_ms}ms
                </span>
              </div>
              <button
                onClick={() => toggleProvider(prov.id, !prov.enabled)}
                className={`w-10 h-5 rounded-full transition-colors relative ${
                  prov.enabled ? "bg-hw-accent" : "bg-hw-border"
                }`}
              >
                <span
                  className={`absolute top-0.5 w-4 h-4 rounded-full bg-white transition-transform ${
                    prov.enabled ? "left-5" : "left-0.5"
                  }`}
                />
              </button>
            </div>
            <div className="grid grid-cols-2 gap-2">
              {prov.feeds.map((feed) => {
                const live = feeds.find((f) => f.feed_id === feed.feed_id);
                return (
                  <div key={feed.feed_id} className="bg-hw-bg rounded px-3 py-2">
                    <p className="text-xs text-hw-text-dim">{feed.name}</p>
                    <p className="text-lg font-bold tabular-nums">
                      {live ? live.display : "--"}
                    </p>
                  </div>
                );
              })}
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}
