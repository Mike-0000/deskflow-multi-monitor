import type { ThemeId } from "./types";

/** Slot-based class maps so one App shell can skin all six example themes. */
export interface ThemeClasses {
  root: string;
  header: string;
  logoWrap: string;
  logoIcon: string;
  title: string;
  subtitle: string;
  badgeConnected: string;
  badgeRunning: string;
  badgeEncrypted: string;
  badgeDotConnected: string;
  badgeDotRunning: string;
  main: string;
  glowA: string;
  glowB: string;
  card: string;
  modeRow: string;
  modeServerActive: string;
  modeServerIdle: string;
  modeClientActive: string;
  modeClientIdle: string;
  modeTitleActive: string;
  modeTitleIdle: string;
  modeDescActive: string;
  modeDescIdle: string;
  modeIconActive: string;
  modeIconIdle: string;
  radioActive: string;
  radioIdle: string;
  infoRow: string;
  infoLabel: string;
  infoChip: string;
  infoChipAccent: string;
  infoChipClient: string;
  actionsRow: string;
  btnStart: string;
  btnStop: string;
  btnGhost: string;
  btnSecondary: string;
  layoutCard: string;
  layoutHeader: string;
  layoutTitle: string;
  layoutSubtitle: string;
  layoutToolbar: string;
  layoutHint: string;
  layoutCanvas: string;
  input: string;
  select: string;
  label: string;
  toggleTrack: string;
  toggleDot: string;
  footer: string;
  footerLink: string;
  footerVersion: string;
  errorBox: string;
  successBox: string;
  divider: string;
  uppercaseBadges: boolean;
}

const darkPro: ThemeClasses = {
  root: "theme-preview w-full h-full min-h-0 overflow-hidden flex flex-col relative font-inter bg-[#030712] text-[#f3f4f6] selection:bg-blue-500/30 selection:text-blue-200",
  header:
    "flex-none px-4 sm:px-8 py-4 sm:py-6 flex flex-wrap items-center justify-between gap-3 border-b border-white/5 bg-gray-950 z-10 relative",
  logoWrap:
    "w-10 h-10 shrink-0 rounded-xl bg-gradient-to-br from-blue-500 to-indigo-600 flex items-center justify-center shadow-lg shadow-blue-500/20 ring-1 ring-white/10",
  logoIcon: "text-white",
  title: "text-xl font-semibold tracking-tight text-white",
  subtitle: "text-sm text-gray-400 mt-0.5 hidden sm:block",
  badgeConnected:
    "flex items-center gap-1.5 px-3 py-1.5 rounded-full bg-emerald-500/10 border border-emerald-500/20 text-emerald-400 text-xs font-medium shrink-0",
  badgeRunning:
    "flex items-center gap-1.5 px-3 py-1.5 rounded-full bg-blue-500/10 border border-blue-500/20 text-blue-400 text-xs font-medium shrink-0",
  badgeEncrypted:
    "flex items-center gap-1.5 px-3 py-1.5 rounded-full bg-purple-500/10 border border-purple-500/20 text-purple-400 text-xs font-medium shrink-0",
  badgeDotConnected: "w-1.5 h-1.5 rounded-full bg-emerald-400",
  badgeDotRunning: "w-1.5 h-1.5 rounded-full bg-blue-400",
  main: "flex-1 min-h-0 overflow-y-auto overflow-x-hidden p-4 sm:p-8 flex flex-col gap-4 sm:gap-6 relative custom-scrollbar",
  // Soft static washes — no live Gaussian blur (major GPU cost on WebView2).
  glowA: "hidden",
  glowB: "hidden",
  card: "bg-[#0b0f19] border border-white/5 rounded-2xl shadow-xl shadow-black/50 overflow-hidden flex flex-col z-10 relative w-full min-w-0",
  modeRow:
    "p-4 sm:p-6 grid grid-cols-1 sm:grid-cols-2 gap-3 sm:gap-4 border-b border-white/5",
  modeServerActive:
    "relative group text-left p-4 sm:p-5 rounded-xl border transition-all focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-offset-[#0b0f19] overflow-hidden min-w-0 border-blue-500/50 bg-blue-500/5 hover:bg-blue-500/10 focus:ring-blue-500",
  modeServerIdle:
    "relative group text-left p-4 sm:p-5 rounded-xl border transition-all focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-offset-[#0b0f19] overflow-hidden min-w-0 border-white/10 bg-white/5 hover:bg-white/10 focus:ring-white/20",
  modeClientActive:
    "text-left p-4 sm:p-5 rounded-xl border transition-all focus:outline-none focus:ring-2 min-w-0 border-blue-500/50 bg-blue-500/5 focus:ring-blue-500 focus:ring-offset-2 focus:ring-offset-[#0b0f19]",
  modeClientIdle:
    "text-left p-4 sm:p-5 rounded-xl border transition-all focus:outline-none focus:ring-2 min-w-0 border-white/10 bg-white/5 hover:bg-white/10 focus:ring-white/20",
  modeTitleActive:
    "text-base font-semibold text-white flex items-center gap-2 flex-wrap",
  modeTitleIdle:
    "text-base font-medium text-gray-300 flex items-center gap-2 flex-wrap",
  modeDescActive: "text-sm text-gray-400 mt-1 break-words",
  modeDescIdle: "text-sm text-gray-500 mt-1 break-words",
  modeIconActive: "text-blue-400",
  modeIconIdle: "text-gray-500",
  radioActive:
    "w-5 h-5 shrink-0 rounded-full border-4 shadow-[0_0_10px_rgba(59,130,246,0.3)] border-blue-500 bg-[#0b0f19]",
  radioIdle:
    "w-5 h-5 shrink-0 rounded-full border-2 border-gray-600 bg-transparent",
  infoRow:
    "px-4 sm:px-6 py-5 bg-black/20 grid grid-cols-1 md:grid-cols-3 gap-4 sm:gap-8",
  infoLabel:
    "text-[11px] font-semibold text-gray-500 uppercase tracking-wider mb-1.5 flex items-center gap-1.5",
  infoChip:
    "text-sm font-jb text-gray-200 bg-white/5 px-2.5 py-1 rounded inline-block border border-white/5 break-all",
  infoChipAccent:
    "bg-blue-500/10 text-blue-300 px-2.5 py-1 rounded border border-blue-500/20 text-sm font-jb break-all",
  infoChipClient:
    "text-sm font-jb text-emerald-300 bg-emerald-500/10 px-2.5 py-1 rounded inline-block border border-emerald-500/20 break-all",
  actionsRow:
    "px-4 sm:px-6 py-4 border-t border-white/5 flex flex-wrap items-center justify-between gap-3 bg-[#0b0f19]",
  btnStart:
    "flex items-center gap-2 px-4 sm:px-5 py-2.5 bg-blue-600 hover:bg-blue-500 text-white text-sm font-medium rounded-lg shadow-lg shadow-blue-600/20 transition-all border border-blue-500/50 disabled:opacity-45 disabled:pointer-events-none",
  btnStop:
    "flex items-center gap-2 px-4 sm:px-5 py-2.5 bg-gray-800 hover:bg-gray-700 text-gray-200 text-sm font-medium rounded-lg transition-all border border-gray-700 disabled:opacity-45 disabled:pointer-events-none",
  btnGhost:
    "flex items-center gap-2 px-3 sm:px-4 py-2.5 bg-transparent hover:bg-white/5 text-gray-400 hover:text-gray-200 text-sm font-medium rounded-lg transition-all disabled:opacity-45 disabled:pointer-events-none",
  btnSecondary:
    "flex items-center gap-2 px-3 sm:px-4 py-2.5 bg-gray-800 hover:bg-gray-700 text-gray-300 text-sm font-medium rounded-lg transition-all border border-gray-700 disabled:opacity-45 disabled:pointer-events-none",
  layoutCard:
    "flex-1 flex flex-col bg-[#0b0f19] border border-white/5 rounded-2xl shadow-xl shadow-black/50 overflow-hidden z-10 relative min-h-[280px] sm:min-h-[420px] w-full min-w-0",
  layoutHeader: "border-b border-white/5 bg-gray-900",
  layoutTitle: "text-lg font-semibold text-white",
  layoutSubtitle: "text-sm text-gray-400 mt-0.5",
  layoutToolbar:
    "px-4 sm:px-6 py-4 border-t border-white/5 bg-black/20 flex flex-wrap xl:flex-nowrap items-end justify-between gap-4 sm:gap-6",
  layoutHint:
    "px-4 sm:px-6 py-2 bg-[#080b12] border-b border-white/5 text-[13px] text-gray-500 break-words",
  layoutCanvas:
    "flex-1 relative overflow-hidden bg-[#05080f] grid-dark-pro min-h-[240px] sm:min-h-[320px]",
  input:
    "w-full max-w-full min-w-0 sm:max-w-xs bg-gray-950 border border-gray-700 rounded-lg pl-9 pr-3 py-2 text-sm text-gray-200 focus:outline-none focus:border-blue-500 focus:ring-1 focus:ring-blue-500 placeholder-gray-600 transition-colors",
  select:
    "appearance-none w-full max-w-full min-w-0 sm:max-w-xs bg-gray-950 border border-gray-700 rounded-lg pl-3 pr-9 py-2 text-sm text-gray-200 font-jb focus:outline-none focus:border-blue-500 focus:ring-1 focus:ring-blue-500 cursor-pointer",
  label:
    "block text-[11px] font-semibold text-gray-500 uppercase tracking-wider mb-2",
  toggleTrack:
    "w-10 h-5 bg-gray-700 rounded-full switch-bg transition-colors duration-200 ease-in-out border border-gray-600 shadow-inner relative",
  toggleDot:
    "switch-dot absolute top-[2px] left-[2px] bg-white border border-gray-300 rounded-full h-4 w-4 transition-transform duration-200 ease-in-out shadow-sm",
  footer:
    "flex-none px-4 sm:px-8 py-3 sm:py-4 border-t border-white/5 flex flex-wrap justify-between items-center gap-3 text-xs text-gray-500 bg-gray-950 z-10 relative",
  footerLink: "hover:text-gray-300 transition-colors whitespace-nowrap",
  footerVersion:
    "font-jb opacity-60 hover:opacity-100 transition-opacity cursor-default break-all",
  errorBox:
    "mx-4 sm:mx-6 mb-4 shrink-0 rounded-lg border border-red-500/30 bg-red-500/10 px-3.5 py-2.5 text-sm text-red-300 break-words",
  successBox:
    "mx-4 sm:mx-6 mb-3 shrink-0 rounded-lg border border-emerald-500/25 bg-emerald-500/10 px-3.5 py-2 text-sm text-emerald-300 break-words",
  divider: "h-8 w-px bg-white/10 hidden sm:block",
  uppercaseBadges: false,
};

const lightMod: ThemeClasses = {
  ...darkPro,
  root: "theme-preview w-full h-full min-h-0 overflow-hidden flex flex-col relative font-nunito bg-[#D9F0F7] text-[#1F2937] selection:bg-[#FFCE1A] selection:text-black",
  header:
    "flex-none px-4 sm:px-8 py-4 sm:py-6 flex flex-wrap items-center justify-between gap-3 z-10 relative bg-transparent",
  logoWrap:
    "w-10 h-10 rounded-2xl bg-[#2A57E8] flex items-center justify-center shadow-md",
  title: "text-2xl font-extrabold tracking-tight text-[#2A57E8]",
  subtitle: "text-sm text-[#2A57E8]/70 font-bold mt-0.5 hidden sm:block",
  badgeConnected:
    "flex items-center gap-1.5 px-4 py-2 rounded-full bg-[#72D328] text-white text-sm font-extrabold shadow-sm shrink-0",
  badgeRunning:
    "flex items-center gap-1.5 px-4 py-2 rounded-full bg-[#2A57E8] text-white text-sm font-extrabold shadow-sm shrink-0",
  badgeEncrypted:
    "flex items-center gap-1.5 px-4 py-2 rounded-full bg-[#A02B9A] text-white text-sm font-extrabold shadow-sm shrink-0",
  badgeDotConnected: "w-1.5 h-1.5 rounded-full bg-white",
  badgeDotRunning: "w-1.5 h-1.5 rounded-full bg-white",
  main: "flex-1 min-h-0 overflow-y-auto overflow-x-hidden p-4 sm:p-8 flex flex-col gap-4 sm:gap-6 relative custom-scrollbar",
  glowA: "hidden",
  glowB: "hidden",
  card: "bg-white border-none rounded-[2rem] shadow-xl overflow-hidden flex flex-col z-10 relative w-full min-w-0",
  modeRow:
    "p-4 sm:p-6 grid grid-cols-1 sm:grid-cols-2 gap-3 sm:gap-4 border-b-2 border-gray-100",
  modeServerActive:
    "relative group text-left p-5 rounded-[1.5rem] border-none transition-all transform hover:scale-[1.01] active:scale-95 shadow-md focus:outline-none bg-[#2A57E8] hover:bg-[#2248c2]",
  modeServerIdle:
    "relative group text-left p-5 rounded-[1.5rem] border-none transition-all shadow-md focus:outline-none bg-[#F4F9FB] hover:bg-[#eef5f8]",
  modeClientActive:
    "text-left p-5 rounded-[1.5rem] border-2 transition-all focus:outline-none bg-[#2A57E8] border-transparent hover:bg-[#2248c2]",
  modeClientIdle:
    "text-left p-5 rounded-[1.5rem] border-2 transition-all focus:outline-none bg-[#F4F9FB] border-transparent hover:bg-[#eef5f8]",
  modeTitleActive: "text-base font-extrabold flex items-center gap-2 text-white",
  modeTitleIdle: "text-base font-extrabold flex items-center gap-2 text-gray-700",
  modeDescActive: "text-sm font-bold mt-1 text-white/80",
  modeDescIdle: "text-sm font-bold mt-1 text-gray-500",
  modeIconActive: "text-white",
  modeIconIdle: "text-gray-500",
  radioActive: "w-6 h-6 rounded-full border-4 border-white bg-[#2A57E8]",
  radioIdle: "w-6 h-6 rounded-full border-4 border-gray-300 bg-transparent",
  infoRow: "px-6 py-5 bg-[#FAFDFF] grid grid-cols-1 md:grid-cols-3 gap-8",
  infoLabel:
    "text-[10px] font-extrabold text-gray-400 uppercase tracking-widest mb-1.5 flex items-center gap-1.5",
  infoChip:
    "text-sm font-jb font-bold text-[#2A57E8] bg-blue-50 px-3 py-1.5 rounded-lg inline-block shadow-sm",
  infoChipAccent:
    "bg-[#72D328] text-white px-3 py-1.5 rounded-lg text-sm font-jb font-bold shadow-sm",
  infoChipClient:
    "text-sm font-jb font-bold text-white bg-[#A02B9A] px-3 py-1.5 rounded-lg inline-block shadow-sm",
  actionsRow:
    "px-4 sm:px-6 py-4 border-t-2 border-gray-100 flex flex-wrap items-center justify-between gap-3 bg-white",
  btnStart:
    "flex items-center gap-2 px-4 sm:px-5 py-2.5 bg-[#2A57E8] hover:bg-[#2248c2] text-white text-sm font-extrabold rounded-xl shadow-md transition-all disabled:opacity-45 disabled:pointer-events-none",
  btnStop:
    "flex items-center gap-2 px-4 sm:px-5 py-2.5 bg-gray-100 hover:bg-gray-200 text-gray-700 text-sm font-extrabold rounded-xl transition-all disabled:opacity-45 disabled:pointer-events-none",
  btnGhost:
    "flex items-center gap-2 px-3 sm:px-4 py-2.5 bg-transparent hover:bg-gray-100 text-gray-500 hover:text-gray-800 text-sm font-extrabold rounded-xl transition-all disabled:opacity-45 disabled:pointer-events-none",
  btnSecondary:
    "flex items-center gap-2 px-3 sm:px-4 py-2.5 bg-[#F4F9FB] hover:bg-[#e8f1f5] text-gray-700 text-sm font-extrabold rounded-xl transition-all border border-gray-100 disabled:opacity-45 disabled:pointer-events-none",
  layoutCard:
    "flex-1 flex flex-col bg-white rounded-[2rem] shadow-xl overflow-hidden z-10 relative min-h-[280px] sm:min-h-[420px] w-full min-w-0",
  layoutHeader: "border-b-2 border-gray-100 bg-white",
  layoutTitle: "text-lg font-extrabold text-[#2A57E8]",
  layoutSubtitle: "text-sm font-bold text-gray-400 mt-0.5",
  layoutToolbar:
    "px-4 sm:px-6 py-4 border-t-2 border-gray-100 bg-[#FAFDFF] flex flex-wrap xl:flex-nowrap items-end justify-between gap-4 sm:gap-6",
  layoutHint:
    "px-4 sm:px-6 py-2 bg-[#F4F9FB] border-b border-gray-100 text-[13px] text-gray-500 font-bold break-words",
  layoutCanvas:
    "flex-1 relative overflow-hidden bg-[#E8F4F8] grid-light-mod min-h-[240px] sm:min-h-[320px]",
  input:
    "w-full max-w-full min-w-0 sm:max-w-xs bg-white border-2 border-gray-100 rounded-xl pl-9 pr-3 py-2 text-sm font-bold text-gray-700 focus:outline-none focus:border-[#2A57E8] placeholder-gray-400 transition-colors shadow-sm",
  select:
    "appearance-none w-full max-w-full min-w-0 sm:max-w-xs bg-white border-2 border-gray-100 rounded-xl pl-3 pr-9 py-2 text-sm font-jb font-bold text-gray-700 focus:outline-none focus:border-[#2A57E8] cursor-pointer shadow-sm",
  label:
    "block text-[10px] font-extrabold text-gray-400 uppercase tracking-widest mb-2",
  toggleTrack:
    "w-10 h-5 bg-gray-200 rounded-full switch-bg transition-colors duration-200 ease-in-out border border-gray-300 shadow-inner relative",
  toggleDot:
    "switch-dot absolute top-[2px] left-[2px] bg-white border border-gray-300 rounded-full h-4 w-4 transition-transform duration-200 ease-in-out shadow-sm",
  footer:
    "flex-none px-4 sm:px-8 py-3 sm:py-4 flex flex-wrap justify-between items-center gap-3 text-xs font-bold text-[#2A57E8]/60 bg-transparent z-10 relative",
  footerLink: "hover:text-[#2A57E8] transition-colors whitespace-nowrap",
  footerVersion: "font-jb opacity-60 break-all",
  errorBox:
    "mx-4 sm:mx-6 mb-4 shrink-0 rounded-xl border-none bg-red-50 px-3.5 py-2.5 text-sm font-bold text-red-600 break-words",
  successBox:
    "mx-4 sm:mx-6 mb-3 shrink-0 rounded-xl border-none bg-emerald-50 px-3.5 py-2 text-sm font-bold text-emerald-700 break-words",
  divider: "h-8 w-px bg-gray-200 hidden sm:block",
  uppercaseBadges: false,
};

const cyberpunk: ThemeClasses = {
  ...darkPro,
  root: "theme-preview w-full h-full min-h-0 overflow-hidden flex flex-col relative font-inter bg-[#31007a] text-white shadow-[inset_0_0_0_8px_#000] selection:bg-[#fbff00] selection:text-black font-black",
  header:
    "flex-none px-4 sm:px-8 py-4 sm:py-6 flex flex-wrap items-center justify-between gap-3 border-b-8 border-black bg-[#0a001a] z-10 relative shadow-[0_8px_0_#39ff14]",
  logoWrap:
    "w-12 h-12 rounded-none bg-[#fbff00] border-4 border-black shadow-[4px_4px_0px_#ff0095] flex items-center justify-center",
  logoIcon: "text-black",
  title: "text-2xl font-black tracking-widest text-white uppercase",
  subtitle:
    "text-xs font-bold text-[#00f0ff] uppercase tracking-wide mt-1 hidden sm:block",
  badgeConnected:
    "flex items-center gap-2 px-3 sm:px-4 py-2 rounded-none bg-[#39ff14] border-4 border-black text-black text-xs font-black uppercase tracking-wider shadow-[4px_4px_0px_#000] shrink-0",
  badgeRunning:
    "flex items-center gap-2 px-3 sm:px-4 py-2 rounded-none bg-[#00f0ff] border-4 border-black text-black text-xs font-black uppercase tracking-wider shadow-[4px_4px_0px_#000] shrink-0",
  badgeEncrypted:
    "flex items-center gap-2 px-3 sm:px-4 py-2 rounded-none bg-[#ff0095] border-4 border-black text-black text-xs font-black uppercase tracking-wider shadow-[4px_4px_0px_#000] shrink-0",
  badgeDotConnected:
    "w-3 h-3 rounded-none border-2 border-black bg-[#ff0095]",
  badgeDotRunning: "w-3 h-3 rounded-none border-2 border-black bg-[#ff5900]",
  main: "flex-1 min-h-0 overflow-y-auto overflow-x-hidden p-4 sm:p-8 flex flex-col gap-4 sm:gap-8 relative custom-scrollbar",
  glowA: "hidden",
  glowB: "hidden",
  card: "bg-[#130030] border-4 border-black rounded-none shadow-[12px_12px_0px_#ff0095] overflow-hidden flex flex-col z-10 relative w-full min-w-0",
  modeRow:
    "p-4 sm:p-6 grid grid-cols-1 sm:grid-cols-2 gap-4 sm:gap-6 border-b-4 border-black",
  modeServerActive:
    "relative group text-left p-6 rounded-none border-4 border-black shadow-[6px_6px_0px_#000] transition-all focus:outline-none overflow-hidden bg-[#39ff14] hover:bg-[#2adb0f]",
  modeServerIdle:
    "relative group text-left p-6 rounded-none border-4 border-black shadow-[6px_6px_0px_#000] transition-all focus:outline-none overflow-hidden bg-[#130030] hover:bg-[#1a0040]",
  modeClientActive:
    "text-left p-6 rounded-none border-4 border-black shadow-[6px_6px_0px_#00f0ff] transition-all focus:outline-none bg-[#39ff14]",
  modeClientIdle:
    "text-left p-6 rounded-none border-4 border-black shadow-[6px_6px_0px_#00f0ff] transition-all focus:outline-none bg-[#130030] hover:bg-[#1a0040]",
  modeTitleActive:
    "text-xl font-black text-black uppercase tracking-widest flex items-center gap-2",
  modeTitleIdle:
    "text-xl font-black text-white uppercase tracking-widest flex items-center gap-2 group-hover:text-[#00f0ff]",
  modeDescActive: "text-xs font-bold text-black mt-2 uppercase",
  modeDescIdle: "text-xs font-bold text-gray-300 mt-2 uppercase",
  modeIconActive: "text-black",
  modeIconIdle: "text-white",
  radioActive:
    "w-6 h-6 rounded-none border-4 border-black bg-[#ff0095] shadow-[4px_4px_0px_#000]",
  radioIdle: "w-6 h-6 rounded-none border-4 border-gray-500 bg-transparent",
  infoRow:
    "px-6 py-5 bg-[#00f0ff] grid grid-cols-1 md:grid-cols-3 gap-8 border-b-4 border-black",
  infoLabel:
    "text-xs font-black text-black uppercase tracking-widest mb-2 flex items-center gap-2",
  infoChip:
    "text-sm font-jb font-black text-black bg-[#fbff00] px-3 py-1.5 border-2 border-black inline-block shadow-[2px_2px_0_#000]",
  infoChipAccent:
    "bg-[#39ff14] text-black px-3 py-1.5 border-2 border-black text-sm font-jb font-black shadow-[2px_2px_0_#000]",
  infoChipClient:
    "text-sm font-jb font-black text-black bg-[#ff0095] px-3 py-1.5 border-2 border-black inline-block shadow-[2px_2px_0_#000]",
  actionsRow:
    "px-4 sm:px-6 py-4 border-t-4 border-black flex flex-wrap items-center justify-between gap-3 bg-[#0a001a]",
  btnStart:
    "flex items-center gap-2 px-4 sm:px-5 py-2.5 bg-[#39ff14] hover:bg-[#2adb0f] text-black text-sm font-black uppercase tracking-wider rounded-none border-4 border-black shadow-[4px_4px_0_#000] transition-all disabled:opacity-45 disabled:pointer-events-none",
  btnStop:
    "flex items-center gap-2 px-4 sm:px-5 py-2.5 bg-[#00f0ff] hover:bg-[#00d4e0] text-black text-sm font-black uppercase tracking-wider rounded-none border-4 border-black shadow-[4px_4px_0_#000] transition-all disabled:opacity-45 disabled:pointer-events-none",
  btnGhost:
    "flex items-center gap-2 px-3 sm:px-4 py-2.5 bg-transparent hover:bg-[#ff0095] text-white hover:text-black text-sm font-black uppercase tracking-wider rounded-none border-4 border-transparent hover:border-black transition-all disabled:opacity-45 disabled:pointer-events-none",
  btnSecondary:
    "flex items-center gap-2 px-3 sm:px-4 py-2.5 bg-[#fbff00] hover:bg-[#e8ec00] text-black text-sm font-black uppercase tracking-wider rounded-none border-4 border-black shadow-[4px_4px_0_#000] transition-all disabled:opacity-45 disabled:pointer-events-none",
  layoutCard:
    "flex-1 flex flex-col bg-[#130030] border-4 border-black rounded-none shadow-[12px_12px_0px_#00f0ff] overflow-hidden z-10 relative min-h-[280px] sm:min-h-[420px] w-full min-w-0",
  layoutHeader: "border-b-4 border-black bg-[#0a001a]",
  layoutTitle: "text-lg font-black text-[#39ff14] uppercase tracking-widest",
  layoutSubtitle: "text-xs font-bold text-[#00f0ff] uppercase mt-1",
  layoutToolbar:
    "px-4 sm:px-6 py-4 border-t-4 border-black bg-[#1a0040] flex flex-wrap xl:flex-nowrap items-end justify-between gap-4 sm:gap-6",
  layoutHint:
    "px-4 sm:px-6 py-2 bg-[#fbff00] border-b-4 border-black text-[13px] text-black font-black uppercase break-words",
  layoutCanvas:
    "flex-1 relative overflow-hidden bg-[#0a001a] grid-cyberpunk min-h-[240px] sm:min-h-[320px]",
  input:
    "w-full max-w-full min-w-0 sm:max-w-xs bg-black border-4 border-[#39ff14] rounded-none pl-9 pr-3 py-2 text-sm font-black text-[#39ff14] focus:outline-none focus:border-[#fbff00] placeholder-[#39ff14]/40 transition-colors",
  select:
    "appearance-none w-full max-w-full min-w-0 sm:max-w-xs bg-black border-4 border-[#00f0ff] rounded-none pl-3 pr-9 py-2 text-sm font-jb font-black text-[#00f0ff] focus:outline-none cursor-pointer",
  label:
    "block text-xs font-black text-[#fbff00] uppercase tracking-widest mb-2",
  toggleTrack:
    "w-10 h-5 bg-black rounded-none switch-bg transition-colors duration-200 ease-in-out border-2 border-[#39ff14] relative",
  toggleDot:
    "switch-dot absolute top-[2px] left-[2px] bg-[#39ff14] border-2 border-black rounded-none h-3.5 w-3.5 transition-transform duration-200 ease-in-out",
  footer:
    "flex-none px-4 sm:px-8 py-3 sm:py-4 border-t-8 border-black flex flex-wrap justify-between items-center gap-3 text-xs font-black uppercase tracking-wider text-[#00f0ff] bg-[#0a001a] z-10 relative",
  footerLink: "hover:text-[#39ff14] transition-colors whitespace-nowrap",
  footerVersion: "font-jb text-[#fbff00] break-all",
  errorBox:
    "mx-4 sm:mx-6 mb-4 shrink-0 rounded-none border-4 border-black bg-[#ff0095] px-3.5 py-2.5 text-sm font-black text-black uppercase break-words",
  successBox:
    "mx-4 sm:mx-6 mb-3 shrink-0 rounded-none border-4 border-black bg-[#39ff14] px-3.5 py-2 text-sm font-black text-black uppercase break-words",
  divider: "h-8 w-1 bg-black hidden sm:block",
  uppercaseBadges: true,
};

const brutalist: ThemeClasses = {
  ...darkPro,
  root: "theme-preview w-full h-full min-h-0 overflow-hidden flex flex-col relative font-inter bg-[#e6e2d6] text-black selection:bg-black selection:text-white",
  header:
    "flex-none px-4 sm:px-8 py-4 sm:py-6 flex flex-wrap items-center justify-between gap-3 border-b-4 border-black bg-[#0bbd38] z-10 relative",
  logoWrap:
    "w-12 h-12 bg-black flex items-center justify-center border-2 border-black shadow-[4px_4px_0px_0px_#fff]",
  logoIcon: "text-white",
  title: "text-3xl font-black tracking-tighter text-black uppercase",
  subtitle:
    "text-xs text-black font-bold uppercase mt-1 tracking-widest hidden sm:block",
  badgeConnected:
    "flex items-center gap-2 px-3 py-1.5 bg-white border-2 border-black text-black text-xs font-black uppercase shadow-[2px_2px_0px_0px_#000] shrink-0",
  badgeRunning:
    "flex items-center gap-2 px-3 py-1.5 bg-[#ff30b4] border-2 border-black text-black text-xs font-black uppercase shadow-[2px_2px_0px_0px_#000] shrink-0",
  badgeEncrypted:
    "flex items-center gap-2 px-3 py-1.5 bg-black border-2 border-black text-white text-xs font-black uppercase shadow-[2px_2px_0px_0px_rgba(255,255,255,1)] shrink-0",
  badgeDotConnected: "w-2.5 h-2.5 bg-[#0bbd38] border-2 border-black",
  badgeDotRunning: "w-2.5 h-2.5 bg-black",
  main: "flex-1 min-h-0 overflow-y-auto overflow-x-hidden p-4 sm:p-8 flex flex-col gap-4 sm:gap-8 relative custom-scrollbar",
  glowA: "hidden",
  glowB: "hidden",
  card: "bg-white border-4 border-black shadow-[8px_8px_0px_0px_#000] overflow-hidden flex flex-col z-10 relative w-full min-w-0",
  modeRow: "grid grid-cols-1 sm:grid-cols-2 border-b-4 border-black gap-0 p-0",
  modeServerActive:
    "relative group text-left p-4 sm:p-6 border-b-4 sm:border-b-0 sm:border-r-4 border-black transition-none focus:outline-none bg-[#0bbd38] min-w-0",
  modeServerIdle:
    "relative group text-left p-4 sm:p-6 border-b-4 sm:border-b-0 sm:border-r-4 border-black transition-none focus:outline-none bg-white hover:bg-[#e6e2d6] min-w-0",
  modeClientActive:
    "text-left p-4 sm:p-6 bg-[#0bbd38] transition-none focus:outline-none min-w-0",
  modeClientIdle:
    "text-left p-4 sm:p-6 bg-transparent hover:bg-[#e6e2d6] transition-none focus:outline-none min-w-0",
  modeTitleActive:
    "text-2xl font-black text-black uppercase flex items-center gap-2",
  modeTitleIdle:
    "text-2xl font-black text-black uppercase flex items-center gap-2",
  modeDescActive: "text-xs font-bold text-black mt-2 uppercase",
  modeDescIdle: "text-xs font-bold text-black mt-2 uppercase",
  modeIconActive: "text-black",
  modeIconIdle: "text-black",
  radioActive: "w-6 h-6 border-4 border-black bg-black",
  radioIdle: "w-6 h-6 border-4 border-black bg-white",
  infoRow: "px-6 py-6 grid grid-cols-1 md:grid-cols-3 gap-8 bg-white",
  infoLabel:
    "text-xs font-black text-black uppercase tracking-widest mb-3 flex items-center gap-2",
  infoChip:
    "text-sm font-jb font-black text-black bg-[#e6e2d6] px-3 py-1.5 border-2 border-black inline-block",
  infoChipAccent:
    "bg-[#0bbd38] text-black px-3 py-1.5 border-2 border-black text-sm font-jb font-black",
  infoChipClient:
    "text-sm font-jb font-black text-black bg-[#ff30b4] px-3 py-1.5 border-2 border-black inline-block",
  actionsRow:
    "px-4 sm:px-6 py-4 border-t-4 border-black flex flex-wrap items-center justify-between gap-3 bg-[#e6e2d6]",
  btnStart:
    "flex items-center gap-2 px-4 sm:px-5 py-2.5 bg-black hover:bg-[#222] text-white text-sm font-black uppercase rounded-none border-2 border-black shadow-[3px_3px_0_#0bbd38] transition-none disabled:opacity-45 disabled:pointer-events-none",
  btnStop:
    "flex items-center gap-2 px-4 sm:px-5 py-2.5 bg-white hover:bg-[#f5f5f0] text-black text-sm font-black uppercase rounded-none border-2 border-black shadow-[3px_3px_0_#000] transition-none disabled:opacity-45 disabled:pointer-events-none",
  btnGhost:
    "flex items-center gap-2 px-3 sm:px-4 py-2.5 bg-transparent hover:bg-black hover:text-white text-black text-sm font-black uppercase rounded-none border-2 border-transparent hover:border-black transition-none disabled:opacity-45 disabled:pointer-events-none",
  btnSecondary:
    "flex items-center gap-2 px-3 sm:px-4 py-2.5 bg-[#ff30b4] hover:bg-[#e028a0] text-black text-sm font-black uppercase rounded-none border-2 border-black shadow-[3px_3px_0_#000] transition-none disabled:opacity-45 disabled:pointer-events-none",
  layoutCard:
    "flex-1 flex flex-col bg-white border-4 border-black shadow-[8px_8px_0px_0px_#000] overflow-hidden z-10 relative min-h-[280px] sm:min-h-[420px] w-full min-w-0",
  layoutHeader: "border-b-4 border-black bg-[#0bbd38]",
  layoutTitle: "text-lg font-black text-black uppercase",
  layoutSubtitle: "text-xs font-bold text-black uppercase mt-1 tracking-widest",
  layoutToolbar:
    "px-4 sm:px-6 py-4 border-t-4 border-black bg-white flex flex-wrap xl:flex-nowrap items-end justify-between gap-4 sm:gap-6",
  layoutHint:
    "px-4 sm:px-6 py-2 bg-[#e6e2d6] border-b-4 border-black text-[13px] text-black font-bold uppercase break-words",
  layoutCanvas:
    "flex-1 relative overflow-hidden bg-[#e6e2d6] grid-brutalist min-h-[240px] sm:min-h-[320px]",
  input:
    "w-full max-w-full min-w-0 sm:max-w-xs bg-white border-2 border-black rounded-none pl-9 pr-3 py-2 text-sm font-bold text-black focus:outline-none focus:shadow-[3px_3px_0_#0bbd38] placeholder-gray-500 transition-none",
  select:
    "appearance-none w-full max-w-full min-w-0 sm:max-w-xs bg-white border-2 border-black rounded-none pl-3 pr-9 py-2 text-sm font-jb font-bold text-black focus:outline-none cursor-pointer",
  label:
    "block text-xs font-black text-black uppercase tracking-widest mb-2",
  toggleTrack:
    "w-10 h-5 bg-white rounded-none switch-bg transition-colors duration-200 ease-in-out border-2 border-black relative",
  toggleDot:
    "switch-dot absolute top-[2px] left-[2px] bg-black border-2 border-black rounded-none h-3.5 w-3.5 transition-transform duration-200 ease-in-out",
  footer:
    "flex-none px-4 sm:px-8 py-3 sm:py-4 border-t-4 border-black flex flex-wrap justify-between items-center gap-3 text-xs font-black uppercase tracking-widest text-black bg-white z-10 relative",
  footerLink: "hover:underline whitespace-nowrap",
  footerVersion: "font-jb break-all",
  errorBox:
    "mx-4 sm:mx-6 mb-4 shrink-0 rounded-none border-2 border-black bg-[#ff30b4] px-3.5 py-2.5 text-sm font-black text-black uppercase break-words",
  successBox:
    "mx-4 sm:mx-6 mb-3 shrink-0 rounded-none border-2 border-black bg-[#0bbd38] px-3.5 py-2 text-sm font-black text-black uppercase break-words",
  divider: "h-8 w-1 bg-black hidden sm:block",
  uppercaseBadges: true,
};

const monochrome: ThemeClasses = {
  ...darkPro,
  root: "theme-preview w-full h-full min-h-0 overflow-hidden flex flex-col relative font-lora bg-[#111111] text-[#eeeeee] selection:bg-[#eeeeee] selection:text-[#111111]",
  header:
    "flex-none px-4 sm:px-10 py-4 sm:py-8 flex flex-wrap items-center justify-between gap-3 sharp-border-b bg-[#111111] z-10 relative",
  logoWrap:
    "w-14 h-14 sharp-border bg-transparent flex items-center justify-center rounded-none",
  logoIcon: "text-[#eeeeee]",
  title:
    "text-4xl font-oswald font-bold text-[#eeeeee] tracking-wider uppercase",
  subtitle: "text-sm font-serif italic text-[#888888] mt-1 hidden sm:block",
  badgeConnected:
    "flex items-center gap-2 px-3 py-1.5 sharp-border text-[#eeeeee] text-xs font-oswald tracking-[0.1em] bg-transparent rounded-none uppercase shrink-0",
  badgeRunning:
    "flex items-center gap-2 px-3 py-1.5 sharp-border text-[#eeeeee] text-xs font-oswald tracking-[0.1em] bg-transparent rounded-none uppercase shrink-0",
  badgeEncrypted:
    "flex items-center gap-2 px-3 py-1.5 sharp-border text-[#eeeeee] text-xs font-oswald tracking-[0.1em] bg-transparent rounded-none uppercase shrink-0",
  badgeDotConnected: "w-2.5 h-2.5 bg-[#eeeeee] rounded-none",
  badgeDotRunning: "w-2.5 h-2.5 bg-[#eeeeee] rounded-none",
  main: "flex-1 min-h-0 overflow-y-auto overflow-x-hidden p-4 sm:p-8 flex flex-col gap-4 sm:gap-8 relative custom-scrollbar",
  glowA: "hidden",
  glowB: "hidden",
  card: "sharp-border bg-[#111111] flex flex-col z-10 relative rounded-none overflow-hidden w-full min-w-0",
  modeRow: "grid grid-cols-1 sm:grid-cols-2 sharp-border-b gap-0 p-0",
  modeServerActive:
    "relative group text-left p-5 sm:p-8 transition-none focus:outline-none sharp-border-b sm:border-b-0 sm:sharp-border-r rounded-none bg-[#eeeeee] text-[#111111] min-w-0",
  modeServerIdle:
    "relative group text-left p-5 sm:p-8 transition-none focus:outline-none sharp-border-b sm:border-b-0 sm:sharp-border-r rounded-none bg-transparent hover:bg-[#222222] min-w-0",
  modeClientActive:
    "text-left p-5 sm:p-8 bg-[#eeeeee] text-[#111111] transition-colors focus:outline-none rounded-none min-w-0",
  modeClientIdle:
    "text-left p-5 sm:p-8 bg-transparent hover:bg-[#222222] transition-colors focus:outline-none rounded-none min-w-0",
  modeTitleActive:
    "text-2xl sm:text-3xl font-oswald font-bold uppercase flex items-center gap-3 flex-wrap",
  modeTitleIdle:
    "text-2xl sm:text-3xl font-oswald font-bold uppercase flex items-center gap-3 flex-wrap text-[#eeeeee]",
  modeDescActive: "text-sm font-serif italic text-[#444444] mt-2 break-words",
  modeDescIdle: "text-sm font-serif italic text-[#888888] mt-2 break-words",
  modeIconActive: "text-[#111111]",
  modeIconIdle: "text-[#eeeeee]",
  radioActive: "w-5 h-5 bg-[#111111] rounded-none mt-1",
  radioIdle: "w-5 h-5 sharp-border bg-transparent rounded-none mt-1",
  infoRow: "p-8 grid grid-cols-1 md:grid-cols-3 gap-12 bg-[#111111]",
  infoLabel:
    "text-xs font-oswald text-[#eeeeee] tracking-[0.2em] uppercase mb-4 flex items-center gap-2",
  infoChip:
    "text-sm font-space text-[#eeeeee] sharp-border px-3 py-1.5 inline-block rounded-none",
  infoChipAccent:
    "text-sm font-space text-[#111111] bg-[#eeeeee] px-3 py-1.5 inline-block rounded-none",
  infoChipClient:
    "text-sm font-space text-[#eeeeee] sharp-border px-3 py-1.5 inline-block rounded-none",
  actionsRow:
    "px-4 sm:px-8 py-4 sm:py-5 sharp-border-t flex flex-wrap items-center justify-between gap-3 bg-[#111111]",
  btnStart:
    "btn-stark flex items-center gap-2 px-4 sm:px-5 py-2.5 text-sm disabled:opacity-45 disabled:pointer-events-none",
  btnStop:
    "btn-stark flex items-center gap-2 px-4 sm:px-5 py-2.5 text-sm disabled:opacity-45 disabled:pointer-events-none",
  btnGhost:
    "flex items-center gap-2 px-3 sm:px-4 py-2.5 bg-transparent hover:bg-[#222] text-[#888] hover:text-[#eeeeee] text-xs font-oswald tracking-[0.1em] uppercase transition-colors rounded-none disabled:opacity-45 disabled:pointer-events-none",
  btnSecondary:
    "btn-stark flex items-center gap-2 px-3 sm:px-4 py-2.5 text-sm disabled:opacity-45 disabled:pointer-events-none",
  layoutCard:
    "flex-1 flex flex-col sharp-border bg-[#111111] overflow-hidden z-10 relative rounded-none min-h-[280px] sm:min-h-[420px] w-full min-w-0",
  layoutHeader: "sharp-border-b bg-[#111111]",
  layoutTitle:
    "text-xl sm:text-2xl font-oswald font-bold text-[#eeeeee] uppercase tracking-wider",
  layoutSubtitle: "text-sm font-serif italic text-[#888888] mt-1",
  layoutToolbar:
    "px-4 sm:px-8 py-4 sm:py-5 sharp-border-t bg-[#111111] flex flex-wrap xl:flex-nowrap items-end justify-between gap-4 sm:gap-6",
  layoutHint:
    "px-4 sm:px-8 py-2 bg-[#0a0a0a] sharp-border-b text-[13px] text-[#666] font-serif italic break-words",
  layoutCanvas:
    "flex-1 relative overflow-hidden bg-[#0a0a0a] grid-monochrome min-h-[240px] sm:min-h-[320px]",
  input:
    "w-full max-w-full min-w-0 sm:max-w-xs bg-transparent sharp-border rounded-none pl-9 pr-3 py-2 text-sm font-space text-[#eeeeee] focus:outline-none focus:bg-[#1a1a1a] placeholder-[#666] transition-colors",
  select:
    "appearance-none w-full max-w-full min-w-0 sm:max-w-xs bg-transparent sharp-border rounded-none pl-3 pr-9 py-2 text-sm font-space text-[#eeeeee] focus:outline-none cursor-pointer",
  label:
    "block text-xs font-oswald text-[#eeeeee] tracking-[0.2em] uppercase mb-2",
  toggleTrack:
    "w-12 h-5 bg-transparent rounded-none switch-bg transition-colors duration-200 ease-in-out border border-[#eeeeee] relative",
  toggleDot:
    "switch-dot absolute top-[2px] left-[2px] bg-[#eeeeee] rounded-none h-3.5 w-3.5 transition-transform duration-200 ease-in-out",
  footer:
    "flex-none px-4 sm:px-10 py-3 sm:py-5 sharp-border-t flex flex-wrap justify-between items-center gap-3 bg-[#111111] z-10 relative",
  footerLink:
    "text-xs font-oswald tracking-[0.1em] text-[#eeeeee] uppercase hover:text-white transition-colors whitespace-nowrap",
  footerVersion:
    "font-space text-xs text-[#666666] hover:text-[#eeeeee] transition-colors cursor-default break-all",
  errorBox:
    "mx-4 sm:mx-8 mb-4 shrink-0 rounded-none sharp-border px-3.5 py-2.5 text-sm font-serif italic text-[#eeeeee] break-words",
  successBox:
    "mx-4 sm:mx-8 mb-3 shrink-0 rounded-none bg-[#eeeeee] text-[#111111] px-3.5 py-2 text-sm font-oswald uppercase tracking-wider break-words",
  divider: "h-8 w-px bg-[#eeeeee]/20 hidden sm:block",
  uppercaseBadges: true,
};

const warm: ThemeClasses = {
  ...darkPro,
  root: "theme-preview w-full h-full min-h-0 overflow-hidden flex flex-col relative font-inter bg-[#6B1F45] text-white selection:bg-[#F97B6B] selection:text-white",
  header:
    "flex-none px-4 sm:px-8 py-4 sm:py-6 flex flex-wrap items-center justify-between gap-3 border-b-[4px] border-[#8E2F5E] bg-[#6B1F45] z-10 relative",
  logoWrap:
    "w-12 h-12 rounded-full bg-[#E3F217] flex items-center justify-center",
  logoIcon: "text-[#6B1F45]",
  title: "text-3xl font-black tracking-tight text-white leading-none",
  subtitle: "text-sm font-bold text-white/70 mt-1 hidden sm:block",
  badgeConnected:
    "flex items-center gap-2 px-3 sm:px-4 py-2 rounded-full bg-[#1ED760] text-[#6B1F45] text-sm font-black uppercase tracking-wide shrink-0",
  badgeRunning:
    "flex items-center gap-2 px-3 sm:px-4 py-2 rounded-full bg-[#286E7A] text-white text-sm font-black uppercase tracking-wide shrink-0",
  badgeEncrypted:
    "flex items-center gap-2 px-3 sm:px-4 py-2 rounded-full bg-[#4A23B2] text-white text-sm font-black uppercase tracking-wide shrink-0",
  badgeDotConnected: "w-2 h-2 rounded-full bg-[#6B1F45]",
  badgeDotRunning: "w-2 h-2 rounded-full bg-[#E3F217]",
  main: "flex-1 min-h-0 overflow-y-auto overflow-x-hidden p-4 sm:p-8 flex flex-col gap-4 sm:gap-8 relative custom-scrollbar bg-[#6B1F45]",
  glowA: "hidden",
  glowB: "hidden",
  card: "bg-[#286E7A] border-[4px] sm:border-[6px] border-[#8E2F5E] rounded-[28px] sm:rounded-[48px] overflow-hidden flex flex-col z-10 relative w-full min-w-0",
  modeRow:
    "p-4 sm:p-8 grid grid-cols-1 sm:grid-cols-2 gap-4 sm:gap-6 border-b-[4px] border-black/10",
  modeServerActive:
    "relative group text-left p-5 sm:p-6 rounded-[24px] sm:rounded-[32px] bg-[#D8CAE6] text-[#6B1F45] transition-all focus:outline-none overflow-hidden min-w-0",
  modeServerIdle:
    "relative group text-left p-5 sm:p-6 rounded-[24px] sm:rounded-[32px] bg-black/10 hover:bg-black/20 transition-all focus:outline-none overflow-hidden min-w-0",
  modeClientActive:
    "text-left p-5 sm:p-6 rounded-[24px] sm:rounded-[32px] bg-[#D8CAE6] text-[#6B1F45] transition-all focus:outline-none min-w-0",
  modeClientIdle:
    "text-left p-5 sm:p-6 rounded-[24px] sm:rounded-[32px] bg-black/10 hover:bg-black/20 transition-all focus:outline-none min-w-0",
  modeTitleActive:
    "text-xl sm:text-2xl font-black text-[#4A23B2] flex items-center gap-3 flex-wrap",
  modeTitleIdle:
    "text-xl sm:text-2xl font-black text-white flex items-center gap-3 flex-wrap",
  modeDescActive:
    "text-sm sm:text-base font-bold text-[#4A23B2]/70 mt-2 leading-tight break-words",
  modeDescIdle:
    "text-sm sm:text-base font-bold text-white/70 mt-2 leading-tight break-words",
  modeIconActive: "text-[#4A23B2]",
  modeIconIdle: "text-white/80",
  radioActive: "w-8 h-8 rounded-full border-[8px] border-[#4A23B2] bg-[#4A23B2]",
  radioIdle: "w-8 h-8 rounded-full border-[4px] border-white/40 bg-transparent",
  infoRow: "px-8 py-6 bg-[#1f5a64] grid grid-cols-1 md:grid-cols-3 gap-8",
  infoLabel:
    "text-[11px] font-black text-white/60 uppercase tracking-wider mb-2 flex items-center gap-1.5",
  infoChip:
    "text-sm font-jb font-bold text-[#6B1F45] bg-[#E3F217] px-3 py-1.5 rounded-full inline-block",
  infoChipAccent:
    "bg-[#F97B6B] text-white px-3 py-1.5 rounded-full text-sm font-jb font-bold",
  infoChipClient:
    "text-sm font-jb font-bold text-white bg-[#4A23B2] px-3 py-1.5 rounded-full inline-block",
  actionsRow:
    "px-4 sm:px-8 py-4 sm:py-5 border-t-[4px] border-black/10 flex flex-wrap items-center justify-between gap-3 bg-[#286E7A]",
  btnStart:
    "flex items-center gap-2 px-4 sm:px-6 py-2.5 sm:py-3 bg-[#E3F217] hover:bg-[#d4e314] text-[#6B1F45] text-sm font-black rounded-full transition-all disabled:opacity-45 disabled:pointer-events-none",
  btnStop:
    "flex items-center gap-2 px-4 sm:px-6 py-2.5 sm:py-3 bg-[#6B1F45] hover:bg-[#5a1a3a] text-white text-sm font-black rounded-full transition-all disabled:opacity-45 disabled:pointer-events-none",
  btnGhost:
    "flex items-center gap-2 px-3 sm:px-5 py-2.5 sm:py-3 bg-transparent hover:bg-white/10 text-white/80 hover:text-white text-sm font-black rounded-full transition-all disabled:opacity-45 disabled:pointer-events-none",
  btnSecondary:
    "flex items-center gap-2 px-3 sm:px-5 py-2.5 sm:py-3 bg-[#4A23B2] hover:bg-[#3a1a8f] text-white text-sm font-black rounded-full transition-all disabled:opacity-45 disabled:pointer-events-none",
  layoutCard:
    "flex-1 flex flex-col bg-[#286E7A] border-[4px] sm:border-[6px] border-[#8E2F5E] rounded-[28px] sm:rounded-[48px] overflow-hidden z-10 relative min-h-[280px] sm:min-h-[420px] w-full min-w-0",
  layoutHeader: "border-b-[4px] border-black/10 bg-[#286E7A]",
  layoutTitle: "text-xl sm:text-2xl font-black text-white",
  layoutSubtitle: "text-sm font-bold text-white/70 mt-1",
  layoutToolbar:
    "px-4 sm:px-8 py-4 sm:py-5 border-t-[4px] border-black/10 bg-[#1f5a64] flex flex-wrap xl:flex-nowrap items-end justify-between gap-4 sm:gap-6",
  layoutHint:
    "px-4 sm:px-8 py-2 bg-[#6B1F45]/40 border-b border-black/10 text-[13px] text-white/70 font-bold break-words",
  layoutCanvas:
    "flex-1 relative overflow-hidden bg-[#6B1F45] grid-warm min-h-[240px] sm:min-h-[320px]",
  input:
    "w-full max-w-full min-w-0 sm:max-w-xs bg-white/10 border-2 border-white/20 rounded-full pl-9 pr-3 py-2 text-sm font-bold text-white focus:outline-none focus:border-[#E3F217] placeholder-white/40 transition-colors",
  select:
    "appearance-none w-full max-w-full min-w-0 sm:max-w-xs bg-white/10 border-2 border-white/20 rounded-full pl-3 pr-9 py-2 text-sm font-jb font-bold text-white focus:outline-none cursor-pointer",
  label:
    "block text-[11px] font-black text-white/60 uppercase tracking-wider mb-2",
  toggleTrack:
    "w-10 h-5 bg-white/20 rounded-full switch-bg transition-colors duration-200 ease-in-out border border-white/30 shadow-inner relative",
  toggleDot:
    "switch-dot absolute top-[2px] left-[2px] bg-[#E3F217] border border-[#6B1F45] rounded-full h-4 w-4 transition-transform duration-200 ease-in-out shadow-sm",
  footer:
    "flex-none px-4 sm:px-8 py-3 sm:py-4 border-t-[4px] border-[#8E2F5E] flex flex-wrap justify-between items-center gap-3 text-xs font-bold text-white/60 bg-[#6B1F45] z-10 relative",
  footerLink: "hover:text-white transition-colors whitespace-nowrap",
  footerVersion: "font-jb text-[#E3F217]/80 break-all",
  errorBox:
    "mx-4 sm:mx-8 mb-4 shrink-0 rounded-[20px] sm:rounded-[24px] border-none bg-[#F97B6B] px-4 py-2.5 text-sm font-black text-white break-words",
  successBox:
    "mx-4 sm:mx-8 mb-3 shrink-0 rounded-[20px] sm:rounded-[24px] border-none bg-[#1ED760] px-4 py-2 text-sm font-black text-[#6B1F45] break-words",
  divider: "h-8 w-px bg-white/20 hidden sm:block",
  uppercaseBadges: true,
};

export const THEME_CLASSES: Record<ThemeId, ThemeClasses> = {
  "dark-pro": darkPro,
  "light-mod": lightMod,
  cyberpunk,
  brutalist,
  monochrome,
  warm,
};

export function themeClasses(id: ThemeId): ThemeClasses {
  return THEME_CLASSES[id] ?? darkPro;
}
