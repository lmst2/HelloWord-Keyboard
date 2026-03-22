import { NavLink } from "react-router-dom";

const navItems = [
  { to: "/", label: "Dashboard", icon: "grid" },
  { to: "/config", label: "Config", icon: "sliders" },
  { to: "/rgb", label: "RGB", icon: "palette" },
  { to: "/eink", label: "E-Ink", icon: "monitor" },
  { to: "/profiles", label: "Profiles", icon: "users" },
  { to: "/data", label: "Data Feed", icon: "activity" },
  { to: "/dfu", label: "Firmware", icon: "download" },
  { to: "/settings", label: "Settings", icon: "settings" },
];

const iconMap: Record<string, string> = {
  grid: "⊞",
  sliders: "⚙",
  palette: "🎨",
  monitor: "📱",
  users: "👤",
  activity: "📊",
  download: "⬇",
  settings: "🔧",
};

export default function Sidebar() {
  return (
    <aside className="w-48 bg-hw-surface border-r border-hw-border flex flex-col shrink-0">
      <div className="px-4 py-5 border-b border-hw-border">
        <h1 className="text-sm font-bold tracking-wide text-hw-accent-light">
          HW-75
        </h1>
        <p className="text-xs text-hw-text-dim mt-0.5">Manager</p>
      </div>
      <nav className="flex-1 py-2 overflow-y-auto">
        {navItems.map((item) => (
          <NavLink
            key={item.to}
            to={item.to}
            className={({ isActive }) =>
              `flex items-center gap-3 px-4 py-2.5 text-sm transition-colors ${
                isActive
                  ? "bg-hw-accent/15 text-hw-accent-light border-r-2 border-hw-accent"
                  : "text-hw-text-dim hover:text-hw-text hover:bg-white/5"
              }`
            }
          >
            <span className="text-base w-5 text-center">
              {iconMap[item.icon]}
            </span>
            {item.label}
          </NavLink>
        ))}
      </nav>
    </aside>
  );
}
