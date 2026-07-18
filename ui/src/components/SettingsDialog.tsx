import type { AppSettings, ProcessMode } from "../types";
import { Modal } from "./ui/Modal";
import { Input } from "./ui/Input";
import { Button } from "./ui/Button";
import { Toggle } from "./ui/Toggle";
import { useEffect, useState } from "react";

const selectClass =
  "w-full rounded-[var(--radius-sm)] border border-[var(--border)] bg-[var(--bg)] px-3 py-2 text-sm text-[var(--text)] outline-none transition-colors focus:border-[var(--accent)] focus:ring-2 focus:ring-[var(--accent-soft)]";

export function SettingsDialog({
  open,
  settings,
  onClose,
  onSave,
}: {
  open: boolean;
  settings: AppSettings | null;
  onClose: () => void;
  onSave: (s: AppSettings) => Promise<void>;
}) {
  const [draft, setDraft] = useState<AppSettings | null>(settings);

  useEffect(() => {
    setDraft(settings);
  }, [settings, open]);

  if (!draft) return null;

  function set<K extends keyof AppSettings>(key: K, value: AppSettings[K]) {
    setDraft((d) => (d ? { ...d, [key]: value } : d));
  }

  return (
    <Modal
      title="Preferences"
      description="General settings for this computer"
      open={open}
      onClose={onClose}
      wide
    >
      <div className="grid gap-4 sm:grid-cols-2">
        <Field label="Computer name">
          <Input
            value={draft.computerName}
            onChange={(e) => set("computerName", e.target.value)}
          />
        </Field>
        <Field label="Port">
          <Input
            type="number"
            value={draft.port}
            onChange={(e) => set("port", Number(e.target.value))}
          />
        </Field>
        <Field label="Bind interface" hint="Leave blank to listen on all interfaces">
          <Input
            value={draft.interface}
            onChange={(e) => set("interface", e.target.value)}
            placeholder="All interfaces"
          />
        </Field>
        <Field label="Log level">
          <select
            className={selectClass}
            value={draft.logLevel}
            onChange={(e) => set("logLevel", e.target.value)}
          >
            {["FATAL", "ERROR", "WARNING", "NOTE", "INFO", "DEBUG", "DEBUG1", "DEBUG2"].map(
              (l) => (
                <option key={l} value={l}>
                  {l}
                </option>
              ),
            )}
          </select>
        </Field>
        <Field
          label="Process mode"
          hint="Desktop: the app starts and stops the core. Service: the Windows service owns the core."
        >
          <select
            className={selectClass}
            value={draft.processMode}
            onChange={(e) => set("processMode", e.target.value as ProcessMode)}
          >
            <option value="desktop">Desktop (app owns core)</option>
            <option value="service">Service (daemon owns core)</option>
          </select>
        </Field>
        <Field label="Protocol">
          <Input
            value={draft.protocol}
            onChange={(e) => set("protocol", e.target.value)}
          />
        </Field>
      </div>

      <div className="mt-5 space-y-1 rounded-[var(--radius-sm)] border border-[var(--border)] bg-[var(--bg)]/40 px-3 py-2">
        <Toggle
          label="TLS encryption"
          checked={draft.tlsEnabled}
          onChange={(v) => set("tlsEnabled", v)}
        />
        <Toggle
          label="Verify peer fingerprints"
          checked={draft.checkPeers}
          onChange={(v) => set("checkPeers", v)}
        />
        <Toggle
          label="Close to system tray"
          checked={draft.closeToTray}
          onChange={(v) => set("closeToTray", v)}
        />
        <Toggle
          label="Start core when the app opens"
          checked={draft.autoStartCore}
          onChange={(v) => set("autoStartCore", v)}
        />
        <Toggle
          label="Prevent system sleep"
          checked={draft.preventSleep}
          onChange={(v) => set("preventSleep", v)}
        />
        <Toggle
          label="Elevate daemon (Windows)"
          checked={draft.elevate}
          onChange={(v) => set("elevate", v)}
        />
      </div>

      <div className="mt-6 flex justify-end gap-2">
        <Button variant="secondary" onClick={onClose}>
          Cancel
        </Button>
        <Button
          onClick={async () => {
            await onSave(draft);
            onClose();
          }}
        >
          Save
        </Button>
      </div>
    </Modal>
  );
}

function Field({
  label,
  hint,
  children,
}: {
  label: string;
  hint?: string;
  children: React.ReactNode;
}) {
  return (
    <label className="block text-xs font-medium text-[var(--text-muted)]">
      <span className="mb-1.5 block">{label}</span>
      {children}
      {hint ? (
        <span className="mt-1.5 block text-[11px] font-normal text-[var(--text-dim)]">
          {hint}
        </span>
      ) : null}
    </label>
  );
}
