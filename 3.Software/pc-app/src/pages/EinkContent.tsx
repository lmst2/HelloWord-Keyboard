import { useRef, useState } from "react";
import { einkUploadImage, einkSendText, einkSwitchApp } from "@/lib/tauri-commands";

const EINK_APPS = [
  { id: 0, name: "Image Display" },
  { id: 1, name: "PC Stats" },
  { id: 2, name: "Info Panel" },
  { id: 3, name: "Scroll Text" },
  { id: 4, name: "Calendar" },
  { id: 5, name: "Quote" },
];

export default function EinkContent() {
  const [text, setText] = useState("");
  const [uploading, setUploading] = useState(false);
  const [selectedApp, setSelectedApp] = useState(0);
  const fileRef = useRef<HTMLInputElement>(null);

  const handleImageUpload = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;
    setUploading(true);
    try {
      const buf = await file.arrayBuffer();
      const bytes = Array.from(new Uint8Array(buf));
      await einkUploadImage(bytes, 0);
    } catch (err) {
      console.error("Upload failed:", err);
    } finally {
      setUploading(false);
    }
  };

  const handleSendText = async () => {
    if (!text.trim()) return;
    try {
      await einkSendText(text);
    } catch (err) {
      console.error("Send text failed:", err);
    }
  };

  const handleSwitchApp = async (appId: number) => {
    setSelectedApp(appId);
    try {
      await einkSwitchApp(appId);
    } catch (err) {
      console.error("Switch app failed:", err);
    }
  };

  return (
    <div className="p-6 overflow-y-auto h-full space-y-6">
      <h2 className="text-lg font-semibold">E-Ink Content</h2>

      {/* Display Mode */}
      <div className="bg-hw-surface border border-hw-border rounded-lg p-4">
        <h3 className="text-sm font-medium text-hw-text-dim mb-3">
          Display Mode
        </h3>
        <div className="grid grid-cols-3 gap-2">
          {EINK_APPS.map((app) => (
            <button
              key={app.id}
              onClick={() => handleSwitchApp(app.id)}
              className={`px-3 py-2 rounded text-sm transition-colors ${
                selectedApp === app.id
                  ? "bg-hw-accent text-white"
                  : "bg-hw-bg text-hw-text-dim hover:text-hw-text"
              }`}
            >
              {app.name}
            </button>
          ))}
        </div>
      </div>

      {/* Image Upload */}
      <div className="bg-hw-surface border border-hw-border rounded-lg p-4">
        <h3 className="text-sm font-medium text-hw-text-dim mb-3">
          Image Upload
        </h3>
        <p className="text-xs text-hw-text-dim mb-3">
          Upload any image — it will be resized to 128×296 and dithered to
          monochrome automatically.
        </p>
        <input
          ref={fileRef}
          type="file"
          accept="image/*"
          onChange={handleImageUpload}
          className="hidden"
        />
        <button
          onClick={() => fileRef.current?.click()}
          disabled={uploading}
          className="px-4 py-2 bg-hw-accent text-white rounded text-sm font-medium hover:bg-hw-accent-light transition-colors disabled:opacity-50"
        >
          {uploading ? "Uploading..." : "Choose Image"}
        </button>
      </div>

      {/* Text Input */}
      <div className="bg-hw-surface border border-hw-border rounded-lg p-4">
        <h3 className="text-sm font-medium text-hw-text-dim mb-3">
          Scroll Text
        </h3>
        <textarea
          value={text}
          onChange={(e) => setText(e.target.value)}
          placeholder="Enter text to display on E-Ink..."
          className="w-full h-24 bg-hw-bg border border-hw-border rounded px-3 py-2 text-sm resize-none"
        />
        <button
          onClick={handleSendText}
          className="mt-2 px-4 py-2 bg-hw-accent text-white rounded text-sm font-medium hover:bg-hw-accent-light transition-colors"
        >
          Send Text
        </button>
      </div>

      {/* Preview */}
      <div className="bg-hw-surface border border-hw-border rounded-lg p-4">
        <h3 className="text-sm font-medium text-hw-text-dim mb-3">
          Preview (128×296)
        </h3>
        <div
          className="border border-hw-border rounded bg-white mx-auto"
          style={{ width: 128, height: 296 }}
        >
          <div className="w-full h-full flex items-center justify-center text-black/30 text-xs">
            Preview
          </div>
        </div>
      </div>
    </div>
  );
}
