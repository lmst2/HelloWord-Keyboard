import { useEffect, useState } from "react";
import {
  ProfileEntry,
  profileList,
  profileSave,
  profileLoad,
  profileDelete,
} from "@/lib/tauri-commands";

export default function ProfileManager() {
  const [profiles, setProfiles] = useState<ProfileEntry[]>([]);
  const [newName, setNewName] = useState("");
  const [selectedSlot, setSelectedSlot] = useState<number | null>(null);
  const [error, setError] = useState<string | null>(null);

  const refresh = async () => {
    try {
      const list = await profileList();
      setProfiles(list);
      setError(null);
    } catch (e) {
      setError(String(e));
    }
  };

  useEffect(() => {
    refresh();
  }, []);

  const handleSave = async () => {
    if (selectedSlot === null || !newName.trim()) return;
    try {
      await profileSave(selectedSlot, newName.trim());
      await refresh();
      setNewName("");
    } catch (e) {
      setError(String(e));
    }
  };

  const handleLoad = async (slot: number) => {
    try {
      await profileLoad(slot);
      setError(null);
    } catch (e) {
      setError(String(e));
    }
  };

  const handleDelete = async (slot: number) => {
    try {
      await profileDelete(slot);
      await refresh();
    } catch (e) {
      setError(String(e));
    }
  };

  return (
    <div className="p-6 overflow-y-auto h-full space-y-6">
      <h2 className="text-lg font-semibold">Profile Manager</h2>

      {error && (
        <div className="bg-hw-error/10 border border-hw-error/30 rounded-lg p-3 text-sm text-hw-error">
          {error}
        </div>
      )}

      {/* Profile List */}
      <div className="bg-hw-surface border border-hw-border rounded-lg overflow-hidden">
        <div className="px-4 py-3 border-b border-hw-border">
          <h3 className="text-sm font-medium text-hw-text-dim">
            Saved Profiles
          </h3>
        </div>
        {profiles.length === 0 ? (
          <p className="px-4 py-6 text-sm text-hw-text-dim text-center">
            No profiles saved. Connect Hub to manage profiles.
          </p>
        ) : (
          <div className="divide-y divide-hw-border">
            {profiles.map((p) => (
              <div
                key={p.slot}
                className="flex items-center justify-between px-4 py-3"
              >
                <div>
                  <span className="text-sm font-medium">{p.name}</span>
                  <span className="text-xs text-hw-text-dim ml-2">
                    Slot {p.slot}
                  </span>
                </div>
                <div className="flex gap-2">
                  <button
                    onClick={() => handleLoad(p.slot)}
                    className="px-3 py-1 bg-hw-accent/20 text-hw-accent-light rounded text-xs hover:bg-hw-accent/30"
                  >
                    Load
                  </button>
                  <button
                    onClick={() => handleDelete(p.slot)}
                    className="px-3 py-1 bg-hw-error/20 text-hw-error rounded text-xs hover:bg-hw-error/30"
                  >
                    Delete
                  </button>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Save New Profile */}
      <div className="bg-hw-surface border border-hw-border rounded-lg p-4 space-y-3">
        <h3 className="text-sm font-medium text-hw-text-dim">
          Save Current Config
        </h3>
        <div className="flex gap-2">
          <select
            value={selectedSlot ?? ""}
            onChange={(e) =>
              setSelectedSlot(
                e.target.value ? Number(e.target.value) : null
              )
            }
            className="bg-hw-bg border border-hw-border rounded px-2 py-1.5 text-sm w-24"
          >
            <option value="">Slot</option>
            {Array.from({ length: 16 }, (_, i) => (
              <option key={i} value={i}>
                Slot {i}
              </option>
            ))}
          </select>
          <input
            type="text"
            placeholder="Profile name"
            value={newName}
            onChange={(e) => setNewName(e.target.value)}
            className="flex-1 bg-hw-bg border border-hw-border rounded px-3 py-1.5 text-sm"
          />
          <button
            onClick={handleSave}
            disabled={selectedSlot === null || !newName.trim()}
            className="px-4 py-1.5 bg-hw-accent text-white rounded text-sm font-medium hover:bg-hw-accent-light disabled:opacity-50"
          >
            Save
          </button>
        </div>
      </div>
    </div>
  );
}
