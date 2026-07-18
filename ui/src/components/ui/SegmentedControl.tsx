import { cn } from "../../lib/utils";

export function SegmentedControl<T extends string>({
  value,
  onChange,
  options,
}: {
  value: T;
  onChange: (v: T) => void;
  options: Array<{ value: T; label: string; description?: string }>;
}) {
  return (
    <div
      role="radiogroup"
      className="grid gap-2 sm:grid-cols-2"
    >
      {options.map((opt) => {
        const active = value === opt.value;
        return (
          <button
            key={opt.value}
            type="button"
            role="radio"
            aria-checked={active}
            onClick={() => onChange(opt.value)}
            className={cn(
              "rounded-[var(--radius)] border px-4 py-3.5 text-left transition-all duration-150",
              active
                ? "border-[var(--accent)] bg-[var(--accent-soft)] shadow-[0_0_0_1px_var(--accent)]"
                : "border-[var(--border)] bg-[var(--bg)]/40 hover:border-[var(--border-strong)] hover:bg-[var(--bg-muted)]/50",
            )}
          >
            <div
              className={cn(
                "text-sm font-semibold",
                active ? "text-[var(--text)]" : "text-[var(--text-muted)]",
              )}
            >
              {opt.label}
            </div>
            {opt.description && (
              <div className="mt-1 text-xs leading-relaxed text-[var(--text-dim)]">
                {opt.description}
              </div>
            )}
          </button>
        );
      })}
    </div>
  );
}
