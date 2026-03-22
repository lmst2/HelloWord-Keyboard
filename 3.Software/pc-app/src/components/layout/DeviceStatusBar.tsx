import { useDeviceStatus } from "@/hooks/useDeviceStatus";

export default function DeviceStatusBar() {
  const status = useDeviceStatus();

  const dot = (connected: boolean) => (
    <span
      className={`inline-block w-2 h-2 rounded-full ${
        connected ? "bg-hw-success" : "bg-hw-error"
      }`}
    />
  );

  return (
    <div className="h-10 bg-hw-surface border-b border-hw-border flex items-center px-4 gap-6 text-xs shrink-0">
      <div className="flex items-center gap-2">
        {dot(status.keyboard === "Connected")}
        <span className="text-hw-text-dim">Keyboard</span>
        <span
          className={
            status.keyboard === "Connected"
              ? "text-hw-success"
              : "text-hw-error"
          }
        >
          {status.keyboard === "Connected" ? "Online" : "Offline"}
        </span>
      </div>
      <div className="flex items-center gap-2">
        {dot(status.hub === "Connected")}
        <span className="text-hw-text-dim">Hub</span>
        <span
          className={
            status.hub === "Connected" ? "text-hw-success" : "text-hw-error"
          }
        >
          {status.hub === "Connected" ? "Online" : "Offline"}
        </span>
      </div>
      <div className="flex-1" />
      <span className="text-hw-text-dim">HelloWord-75 Manager v1.0</span>
    </div>
  );
}
