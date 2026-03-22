import { useCallback, useEffect, useState } from "react";
import { DeviceStatus, getDeviceStatus, startDiscovery } from "@/lib/tauri-commands";

const POLL_INTERVAL = 2000;

export function useDeviceStatus() {
  const [status, setStatus] = useState<DeviceStatus>({
    keyboard: "Disconnected",
    hub: "Disconnected",
    kb_firmware_version: null,
    hub_firmware_version: null,
  });

  const refresh = useCallback(async () => {
    try {
      const s = await getDeviceStatus();
      setStatus(s);
    } catch {
      // Backend not ready yet
    }
  }, []);

  useEffect(() => {
    startDiscovery().catch(() => {});
    refresh();
    const id = setInterval(async () => {
      try {
        await startDiscovery();
      } catch {
        // ignore
      }
      refresh();
    }, POLL_INTERVAL);
    return () => clearInterval(id);
  }, [refresh]);

  return status;
}
