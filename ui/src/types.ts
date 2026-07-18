export type CoreMode = "none" | "client" | "server";
export type ProcessMode = "desktop" | "service";

export type ProcessState =
  | "stopped"
  | "starting"
  | "started"
  | "stopping"
  | "retryPending";

export type ConnectionState =
  | "disconnected"
  | "connecting"
  | "connected"
  | "listening";

export interface CoreStatus {
  processState: ProcessState;
  connectionState: ConnectionState;
  connectedClients: string[];
  secureSocket: string | null;
  mode: CoreMode;
  processMode: ProcessMode;
  versionId: string;
  localIps: string[];
  computerName: string;
  peerFingerprint: string | null;
  retryIn: number | null;
  lastError: string | null;
}

export interface AppSettings {
  coreMode: CoreMode;
  processMode: ProcessMode;
  remoteHost: string;
  computerName: string;
  port: number;
  interface: string;
  logLevel: string;
  closeToTray: boolean;
  autoStartCore: boolean;
  tlsEnabled: boolean;
  checkPeers: boolean;
  elevate: boolean;
  preventSleep: boolean;
  languageSync: boolean;
  invertYScroll: boolean;
  invertXScroll: boolean;
  yScrollScale: number;
  xScrollScale: number;
  dynamicConnectionRetry: boolean;
  externalConfig: boolean;
  externalConfigFile: string;
  protocol: string;
  heartbeat: number;
  hasHeartbeat: boolean;
  relativeMouseMoves: boolean;
  switchDelay: number;
  hasSwitchDelay: boolean;
  clipboardSharing: boolean;
  clipboardSharingSize: number;
  raw: Record<string, string>;
}

export interface ScreenCell {
  name: string;
  aliases: string[];
  isServer: boolean;
  column: number;
  row: number;
}

export interface DisplayRect {
  id: string;
  name: string;
  worldX: number;
  worldY: number;
  width: number;
  height: number;
  localX: number;
  localY: number;
  scale: number;
  dpi: number;
  layoutWidth?: number;
  layoutHeight?: number;
  needsPlacement?: boolean;
}

export interface MachineLayout {
  name: string;
  monitors: DisplayRect[];
}

export interface WorkspaceLayout {
  enabled: boolean;
  version?: number;
  machines: MachineLayout[];
}

export interface ServerConfig {
  columns: number;
  rows: number;
  screens: ScreenCell[];
  protocol: string;
  hasHeartbeat: boolean;
  heartbeat: number;
  relativeMouseMoves: boolean;
  win32KeepForeground: boolean;
  hasSwitchDelay: boolean;
  switchDelay: number;
  hasSwitchDoubleTap: boolean;
  switchDoubleTap: number;
  switchCornerSize: number;
  clipboardSharing: boolean;
  clipboardSharingSize: number;
  defaultLockToScreen: boolean;
  disableLockToScreen: boolean;
  workspaceLayout: WorkspaceLayout;
}

export interface VersionInfo {
  version: string;
  gitSha: string;
  versionId: string;
  appName: string;
}

export interface IpcEventMessage {
  type: string;
  command?: string;
  args?: string;
  reason?: string;
  serverVersion?: string;
}
