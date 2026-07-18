import type { ServerConfig } from "../types";
import { Modal } from "./ui/Modal";
import { Input } from "./ui/Input";
import { Button } from "./ui/Button";
import { Toggle } from "./ui/Toggle";
import { useEffect, useState } from "react";

export function ServerOptionsDialog({
  open,
  config,
  onClose,
  onSave,
}: {
  open: boolean;
  config: ServerConfig | null;
  onClose: () => void;
  onSave: (c: ServerConfig) => Promise<void>;
}) {
  const [draft, setDraft] = useState(config);
  useEffect(() => setDraft(config), [config, open]);
  if (!draft) return null;

  return (
    <Modal
      title="Server options"
      description="How clients connect and switch between screens"
      open={open}
      onClose={onClose}
      wide
    >
      <div className="grid gap-4 sm:grid-cols-2">
        <label className="block text-xs font-medium text-[var(--text-muted)]">
          <span className="mb-1.5 block">Protocol</span>
          <Input
            value={draft.protocol}
            onChange={(e) => setDraft({ ...draft, protocol: e.target.value })}
          />
        </label>
        <label className="block text-xs font-medium text-[var(--text-muted)]">
          <span className="mb-1.5 block">Switch corner size</span>
          <Input
            type="number"
            value={draft.switchCornerSize}
            onChange={(e) =>
              setDraft({ ...draft, switchCornerSize: Number(e.target.value) })
            }
          />
        </label>
        <label className="block text-xs font-medium text-[var(--text-muted)]">
          <span className="mb-1.5 block">Clipboard size limit</span>
          <Input
            type="number"
            value={draft.clipboardSharingSize}
            onChange={(e) =>
              setDraft({
                ...draft,
                clipboardSharingSize: Number(e.target.value),
              })
            }
          />
        </label>
        <label className="block text-xs font-medium text-[var(--text-muted)]">
          <span className="mb-1.5 block">
            Heartbeat {draft.hasHeartbeat ? "(ms)" : ""}
          </span>
          <div className="flex items-center gap-2">
            <input
              type="checkbox"
              className="size-4 accent-[var(--accent)]"
              checked={draft.hasHeartbeat}
              onChange={(e) =>
                setDraft({ ...draft, hasHeartbeat: e.target.checked })
              }
            />
            <Input
              type="number"
              disabled={!draft.hasHeartbeat}
              value={draft.heartbeat}
              onChange={(e) =>
                setDraft({ ...draft, heartbeat: Number(e.target.value) })
              }
            />
          </div>
        </label>
        <label className="block text-xs font-medium text-[var(--text-muted)]">
          <span className="mb-1.5 block">
            Switch delay {draft.hasSwitchDelay ? "(ms)" : ""}
          </span>
          <div className="flex items-center gap-2">
            <input
              type="checkbox"
              className="size-4 accent-[var(--accent)]"
              checked={draft.hasSwitchDelay}
              onChange={(e) =>
                setDraft({ ...draft, hasSwitchDelay: e.target.checked })
              }
            />
            <Input
              type="number"
              disabled={!draft.hasSwitchDelay}
              value={draft.switchDelay}
              onChange={(e) =>
                setDraft({ ...draft, switchDelay: Number(e.target.value) })
              }
            />
          </div>
        </label>
      </div>

      <div className="mt-5 space-y-1 rounded-[var(--radius-sm)] border border-[var(--border)] bg-[var(--bg)]/40 px-3 py-2">
        <Toggle
          label="Relative mouse moves"
          checked={draft.relativeMouseMoves}
          onChange={(v) => setDraft({ ...draft, relativeMouseMoves: v })}
        />
        <Toggle
          label="Keep foreground (Windows)"
          checked={draft.win32KeepForeground}
          onChange={(v) => setDraft({ ...draft, win32KeepForeground: v })}
        />
        <Toggle
          label="Clipboard sharing"
          checked={draft.clipboardSharing}
          onChange={(v) => setDraft({ ...draft, clipboardSharing: v })}
        />
        <Toggle
          label="Default lock to screen"
          checked={draft.defaultLockToScreen}
          onChange={(v) => setDraft({ ...draft, defaultLockToScreen: v })}
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
