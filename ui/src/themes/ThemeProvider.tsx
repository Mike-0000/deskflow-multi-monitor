import {
  createContext,
  useCallback,
  useContext,
  useMemo,
  useState,
  type ReactNode,
} from "react";
import { themeClasses, type ThemeClasses } from "./classes";
import {
  DEFAULT_THEME,
  THEME_STORAGE_KEY,
  type ThemeId,
} from "./types";

function readStoredTheme(): ThemeId {
  try {
    const raw = localStorage.getItem(THEME_STORAGE_KEY);
    if (
      raw === "dark-pro" ||
      raw === "light-mod" ||
      raw === "cyberpunk" ||
      raw === "brutalist" ||
      raw === "monochrome" ||
      raw === "warm"
    ) {
      return raw;
    }
  } catch {
    /* ignore */
  }
  return DEFAULT_THEME;
}

interface ThemeContextValue {
  themeId: ThemeId;
  setThemeId: (id: ThemeId) => void;
  tc: ThemeClasses;
}

const ThemeContext = createContext<ThemeContextValue | null>(null);

export function ThemeProvider({ children }: { children: ReactNode }) {
  const [themeId, setThemeIdState] = useState<ThemeId>(readStoredTheme);

  const setThemeId = useCallback((id: ThemeId) => {
    setThemeIdState(id);
    try {
      localStorage.setItem(THEME_STORAGE_KEY, id);
    } catch {
      /* ignore */
    }
  }, []);

  const value = useMemo(
    () => ({
      themeId,
      setThemeId,
      tc: themeClasses(themeId),
    }),
    [themeId, setThemeId],
  );

  return (
    <ThemeContext.Provider value={value}>{children}</ThemeContext.Provider>
  );
}

export function useTheme() {
  const ctx = useContext(ThemeContext);
  if (!ctx) {
    throw new Error("useTheme must be used within ThemeProvider");
  }
  return ctx;
}
