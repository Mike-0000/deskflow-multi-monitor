import { cn } from "../../lib/utils";

export function Badge({
  children,
  tone = "neutral",
  className,
  pulse,
}: {
  children: React.ReactNode;
  tone?: "neutral" | "success" | "warning" | "danger" | "info";
  className?: string;
  pulse?: boolean;
}) {
  const tones = {
    neutral: "bg-white/[0.06] text-[var(--text-muted)] ring-1 ring-inset ring-white/[0.06]",
    success: "bg-[var(--success-soft)] text-[var(--success)] ring-1 ring-inset ring-emerald-500/20",
    warning: "bg-[var(--warning-soft)] text-[var(--warning)] ring-1 ring-inset ring-amber-500/20",
    danger: "bg-[var(--danger-soft)] text-[var(--danger)] ring-1 ring-inset ring-red-500/20",
    info: "bg-[var(--accent-soft)] text-[var(--accent)] ring-1 ring-inset ring-sky-500/20",
  };

  const dots = {
    neutral: "bg-[var(--text-muted)]",
    success: "bg-[var(--success)]",
    warning: "bg-[var(--warning)]",
    danger: "bg-[var(--danger)]",
    info: "bg-[var(--accent)]",
  };

  return (
    <span
      className={cn(
        "inline-flex items-center gap-1.5 rounded-full px-2.5 py-1 text-xs font-medium",
        tones[tone],
        className,
      )}
    >
      <span
        className={cn(
          "size-1.5 shrink-0 rounded-full",
          dots[tone],
          pulse && "animate-pulse-dot",
        )}
      />
      {children}
    </span>
  );
}
