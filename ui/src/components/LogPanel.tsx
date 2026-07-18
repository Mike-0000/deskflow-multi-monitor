import { useEffect, useRef } from "react";
import { Modal } from "./ui/Modal";
import { Button } from "./ui/Button";
import { Trash2 } from "lucide-react";

export function LogPanel({
  open,
  onClose,
  lines,
  onClear,
}: {
  open: boolean;
  onClose: () => void;
  lines: string[];
  onClear?: () => void;
}) {
  const endRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (open) {
      endRef.current?.scrollIntoView({ behavior: "smooth" });
    }
  }, [lines, open]);

  return (
    <Modal
      title="Debug log"
      description="Live output from deskflow-core. Useful when troubleshooting."
      open={open}
      onClose={onClose}
      wide
    >
      <div className="mb-3 flex items-center justify-between gap-2">
        <p className="text-xs text-[var(--text-dim)]">
          {lines.length === 0
            ? "No output yet"
            : `${lines.length} line${lines.length === 1 ? "" : "s"}`}
        </p>
        {onClear && (
          <Button variant="ghost" size="sm" onClick={onClear} disabled={lines.length === 0}>
            <Trash2 className="size-3.5" />
            Clear
          </Button>
        )}
      </div>
      <pre className="max-h-[min(55vh,480px)] min-h-[240px] overflow-auto rounded-[var(--radius-sm)] border border-[var(--border)] bg-[var(--bg)] p-3 font-mono text-[11px] leading-relaxed text-[#c8d3e0]">
        {lines.length === 0 ? "Waiting for core output…" : lines.join("\n")}
        <div ref={endRef} />
      </pre>
    </Modal>
  );
}
