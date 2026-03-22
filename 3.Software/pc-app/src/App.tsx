import { BrowserRouter, Route, Routes } from "react-router-dom";
import Sidebar from "./components/layout/Sidebar";
import DeviceStatusBar from "./components/layout/DeviceStatusBar";
import Dashboard from "./pages/Dashboard";
import ConfigEditor from "./pages/ConfigEditor";
import RgbControl from "./pages/RgbControl";
import EinkContent from "./pages/EinkContent";
import ProfileManager from "./pages/ProfileManager";
import DataProviders from "./pages/DataProviders";
import DfuManager from "./pages/DfuManager";
import AppSettingsPage from "./pages/AppSettingsPage";

export default function App() {
  return (
    <BrowserRouter>
      <div className="flex h-screen w-screen bg-hw-bg text-hw-text overflow-hidden">
        <Sidebar />
        <div className="flex flex-col flex-1 min-w-0">
          <DeviceStatusBar />
          <main className="flex-1 overflow-hidden">
            <Routes>
              <Route path="/" element={<Dashboard />} />
              <Route path="/config" element={<ConfigEditor />} />
              <Route path="/rgb" element={<RgbControl />} />
              <Route path="/eink" element={<EinkContent />} />
              <Route path="/profiles" element={<ProfileManager />} />
              <Route path="/data" element={<DataProviders />} />
              <Route path="/dfu" element={<DfuManager />} />
              <Route path="/settings" element={<AppSettingsPage />} />
            </Routes>
          </main>
        </div>
      </div>
    </BrowserRouter>
  );
}
