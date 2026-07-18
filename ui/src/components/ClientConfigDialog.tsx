import type { AppSettings } from "../types";
import { Modal } from "./ui/Modal";
import { Input } from "./ui/Input";
import { Button } from "./ui/Button";
import { Toggle } from "./ui/Toggle";
import { useEffect, useState } from "react";

export function ClientConfigDialog({
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
  const [draft, setDraft] = useState(settings);
  useEffect(() => setDraft(settings), [settings, open]);
  if (!draft) return null;

  return (
    <Modal
      title="Client options"
      description="Behavior when this computer connects to a server"
      open={open}
      onClose={onClose}
    >
      <div className="space-y-1 rounded-[var(--radius-sm)] border border-[var(--border)] bg-[var(--bg)]/40 px-3 py-2">
        <Toggle
          label="Dynamic connection retry"
          hint="Automatically retry with backoff when the server is unreachable"
          checked={draft.dynamicConnectionRetry}
          onChange={(v) => setDraft({ ...draft, dynamicConnectionRetry: v })}
        />
        <Toggle
          label="Language sync"
          checked={draft.languageSync}
          onChange={(v) => setDraft({ ...draft, languageSync: v })}
        />
        <Toggle
          label="Invert vertical scroll"
          checked={draft.invertYScroll}
          onChange={(v) => setDraft({ ...draft, invertYScroll: v })}
        />
        <Toggle
          label="Invert horizontal scroll"
          checked={draft.invertXScroll}
          onChange={(v) => setDraft({ ...draft, invertXScroll: v })}
        />
      </div>

      <div className="mt-4 grid gap-3 sm:grid-cols-2">
        <label className="block text-xs font-medium text-[var(--text-muted)]">
          <span className="mb-1.5 block">Vertical scroll scale</span>
          <Input
            type="number"
            step="0.1"
            value={draft.yScrollScale}
            onChange={(e) =>
              setDraft({ ...draft, yScrollScale: Number(e.target.value) })
            }
          />
        </label>
        <label className="block text-xs font-medium text-[var(--text-muted)]">
          <span className="mb-1.5 block">Horizontal scroll scale</span>
          <Input
            type="number"
            step="0.1"
            value={draft.xScrollScale}
            onChange={(e) =>
              setDraft({ ...draft, xScrollScale: Number(e.target.value) })
            }
          />
        </label>
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
