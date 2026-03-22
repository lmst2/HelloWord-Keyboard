import { useCallback, useEffect, useState } from "react";
import { FeedData, dataGetLive, dataStart, dataStop } from "@/lib/tauri-commands";

export function useDataFeed(intervalMs = 1000) {
  const [feeds, setFeeds] = useState<FeedData[]>([]);
  const [running, setRunning] = useState(false);

  const start = useCallback(async () => {
    await dataStart();
    setRunning(true);
  }, []);

  const stop = useCallback(async () => {
    await dataStop();
    setRunning(false);
  }, []);

  useEffect(() => {
    if (!running) return;
    const id = setInterval(async () => {
      try {
        const data = await dataGetLive();
        setFeeds(data);
      } catch {
        // ignore
      }
    }, intervalMs);
    return () => clearInterval(id);
  }, [running, intervalMs]);

  return { feeds, running, start, stop };
}
