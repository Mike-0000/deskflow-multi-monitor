export type ThemeId =
  | "dark-pro"
  | "light-mod"
  | "cyberpunk"
  | "brutalist"
  | "monochrome"
  | "warm";

export interface ThemeMeta {
  id: ThemeId;
  name: string;
  description: string;
  swatch: string[];
}

export const THEMES: ThemeMeta[] = [
  {
    id: "dark-pro",
    name: "Dark Pro",
    description: "Default — deep slate with soft blue accents",
    swatch: ["#030712", "#0b0f19", "#3b82f6", "#10b981"],
  },
  {
    id: "light-mod",
    name: "Light Mod",
    description: "Bright sky blue with bold rounded cards",
    swatch: ["#D9F0F7", "#ffffff", "#2A57E8", "#72D328"],
  },
  {
    id: "cyberpunk",
    name: "Cyberpunk",
    description: "Neon hard-shadow panels on violet",
    swatch: ["#31007a", "#130030", "#39ff14", "#ff0095"],
  },
  {
    id: "brutalist",
    name: "Brutalist",
    description: "High-contrast green header, hard edges",
    swatch: ["#e6e2d6", "#0bbd38", "#000000", "#ff30b4"],
  },
  {
    id: "monochrome",
    name: "Monochrome",
    description: "Stark black and white with Oswald type",
    swatch: ["#111111", "#eeeeee", "#666666", "#222222"],
  },
  {
    id: "warm",
    name: "Warm",
    description: "Berry and teal with soft oversized shapes",
    swatch: ["#6B1F45", "#286E7A", "#E3F217", "#4A23B2"],
  },
];

export const DEFAULT_THEME: ThemeId = "dark-pro";
export const THEME_STORAGE_KEY = "deskflow-ui-theme";
