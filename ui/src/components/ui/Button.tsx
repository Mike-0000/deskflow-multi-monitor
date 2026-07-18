import { cn } from "../../lib/utils";
import type { ButtonHTMLAttributes } from "react";

type Variant = "primary" | "secondary" | "danger" | "ghost" | "soft";
type Size = "sm" | "md" | "lg";

const variants: Record<Variant, string> = {
  primary:
    "bg-[var(--accent)] hover:bg-[var(--accent-hover)] text-white shadow-sm",
  secondary:
    "bg-[var(--bg-muted)] hover:bg-[var(--bg-hover)] text-[var(--text)] border border-[var(--border)]",
  soft: "bg-[var(--accent-soft)] hover:bg-[rgba(59,158,255,0.2)] text-[var(--accent)]",
  danger: "bg-[var(--danger-soft)] hover:bg-[rgba(240,113,120,0.25)] text-[var(--danger)]",
  ghost: "bg-transparent hover:bg-white/[0.04] text-[var(--text-muted)] hover:text-[var(--text)]",
};

const sizes: Record<Size, string> = {
  sm: "h-8 px-2.5 text-xs rounded-[var(--radius-sm)]",
  md: "h-9 px-3.5 text-sm rounded-[var(--radius-sm)]",
  lg: "h-11 px-5 text-sm rounded-[var(--radius)]",
};

export function Button({
  className,
  variant = "primary",
  size = "md",
  ...props
}: ButtonHTMLAttributes<HTMLButtonElement> & {
  variant?: Variant;
  size?: Size;
}) {
  return (
    <button
      className={cn(
        "inline-flex items-center justify-center gap-2 font-medium transition-colors duration-150 disabled:opacity-45 disabled:pointer-events-none active:scale-[0.98]",
        variants[variant],
        sizes[size],
        className,
      )}
      {...props}
    />
  );
}
