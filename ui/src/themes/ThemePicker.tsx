import { Check, Palette, X } from "lucide-react";
import { useTheme } from "./ThemeProvider";
import { THEMES, type ThemeId } from "./types";

export function ThemePicker({
  open,
  onClose,
}: {
  open: boolean;
  onClose: () => void;
}) {
  const { themeId, setThemeId } = useTheme();

  if (!open) return null;

  function pick(id: ThemeId) {
    setThemeId(id);
    onClose();
  }

  return (
    <div
      className="fixed inset-0 z-[100] flex items-center justify-center bg-black/60 p-4"
      role="dialog"
      aria-modal="true"
      aria-labelledby="theme-picker-title"
      onClick={onClose}
    >
      <div
        className="w-full max-w-2xl overflow-hidden rounded-2xl border border-white/10 bg-[#0b0f19] shadow-2xl shadow-black/50"
        onClick={(e) => e.stopPropagation()}
      >
        <div className="flex items-center justify-between border-b border-white/5 px-6 py-4">
          <div className="flex items-center gap-3">
            <div className="flex size-9 items-center justify-center rounded-lg bg-blue-500/15 text-blue-400">
              <Palette className="size-4" />
            </div>
            <div>
              <h2
                id="theme-picker-title"
                className="text-base font-semibold text-white"
              >
                Theme
              </h2>
              <p className="text-xs text-gray-400">
                Choose a visual style for the Deskflow UI
              </p>
            </div>
          </div>
          <button
            type="button"
            onClick={onClose}
            className="rounded-lg p-2 text-gray-400 transition hover:bg-white/5 hover:text-white"
            aria-label="Close"
          >
            <X className="size-4" />
          </button>
        </div>

        <div className="grid gap-3 p-6 sm:grid-cols-2">
          {THEMES.map((theme) => {
            const active = theme.id === themeId;
            return (
              <button
                key={theme.id}
                type="button"
                onClick={() => pick(theme.id)}
                className={`group relative rounded-xl border p-4 text-left transition ${
                  active
                    ? "border-blue-500/50 bg-blue-500/10"
                    : "border-white/10 bg-white/[0.03] hover:border-white/20 hover:bg-white/[0.06]"
                }`}
              >
                <div className="mb-3 flex gap-1.5">
                  {theme.swatch.map((color) => (
                    <span
                      key={color}
                      className="h-6 flex-1 rounded-md ring-1 ring-black/20"
                      style={{ backgroundColor: color }}
                    />
                  ))}
                </div>
                <div className="flex items-start justify-between gap-2">
                  <div>
                    <div className="text-sm font-semibold text-white">
                      {theme.name}
                      {theme.id === "dark-pro" && (
                        <span className="ml-2 text-[10px] font-medium uppercase tracking-wider text-blue-400">
                          Default
                        </span>
                      )}
                    </div>
                    <p className="mt-0.5 text-xs text-gray-400">
                      {theme.description}
                    </p>
                  </div>
                  {active && (
                    <span className="flex size-6 shrink-0 items-center justify-center rounded-full bg-blue-500 text-white">
                      <Check className="size-3.5" strokeWidth={3} />
                    </span>
                  )}
                </div>
              </button>
            );
          })}
        </div>
      </div>
    </div>
  );
}
