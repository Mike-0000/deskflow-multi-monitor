import { Modal } from "./ui/Modal";
import { Button } from "./ui/Button";
import { ShieldCheck } from "lucide-react";

export function TrustPrompt({
  fingerprint,
  onTrust,
  onDismiss,
}: {
  fingerprint: string | null;
  onTrust: () => void;
  onDismiss: () => void;
}) {
  return (
    <Modal
      title="Trust this computer?"
      description="A peer presented this TLS fingerprint. Confirm it matches before continuing."
      open={!!fingerprint}
      onClose={onDismiss}
    >
      <code className="mb-5 block break-all rounded-[var(--radius-sm)] border border-[var(--border)] bg-[var(--bg)] p-3.5 font-mono text-xs leading-relaxed text-[var(--text)]">
        {fingerprint}
      </code>
      <div className="flex justify-end gap-2">
        <Button variant="secondary" onClick={onDismiss}>
          Not now
        </Button>
        <Button onClick={onTrust}>
          <ShieldCheck className="size-4" />
          Trust & continue
        </Button>
      </div>
    </Modal>
  );
}
