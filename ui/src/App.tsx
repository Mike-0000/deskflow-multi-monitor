import { invoke } from "@tauri-apps/api/core";
import { listen } from "@tauri-apps/api/event";
import { openUrl } from "@tauri-apps/plugin-opener";
import {
  CircleHelp,
  Eye,
  EyeOff,
  Lock,
  Monitor,
  Palette,
  Play,
  RotateCcw,
  Settings2,
  Square,
  Terminal,
  User,
} from "lucide-react";
import { useCallback, useEffect, useMemo, useState } from "react";
import { LayoutEditor } from "./components/LayoutEditor";
import { SettingsDialog } from "./components/SettingsDialog";
import { ClientConfigDialog } from "./components/ClientConfigDialog";
import { ServerOptionsDialog } from "./components/ServerOptionsDialog";
import { TrustPrompt } from "./components/TrustPrompt";
import { LogPanel } from "./components/LogPanel";
import { ThemePicker, useTheme } from "./themes";
import type {
  AppSettings,
  ConnectionState,
  CoreMode,
  CoreStatus,
  ProcessState,
  ServerConfig,
  VersionInfo,
} from "./types";

const MAX_LOG = 800;

function connectionLabel(state: ConnectionState): string {
  switch (state) {
    case "connected":
      return "Connected";
    case "listening":
      return "Waiting for clients";
    case "connecting":
      return "Connecting…";
    default:
      return "Not connected";
  }
}

function processLabel(state: ProcessState | undefined): string {
  switch (state) {
    case "started":
      return "Running";
    case "starting":
      return "Starting…";
    case "stopping":
      return "Stopping…";
    case "retryPending":
      return "Retrying…";
    default:
      return "Stopped";
  }
}

function badgeText(label: string, uppercase: boolean) {
  return uppercase ? label.toUpperCase() : label;
}

export default function App() {
  const { themeId, tc } = useTheme();
  const [status, setStatus] = useState<CoreStatus | null>(null);
  const [settings, setSettings] = useState<AppSettings | null>(null);
  const [serverConfig, setServerConfig] = useState<ServerConfig | null>(null);
  const [version, setVersion] = useState<VersionInfo | null>(null);
  const [logs, setLogs] = useState<string[]>([]);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [showSettings, setShowSettings] = useState(false);
  const [showClient, setShowClient] = useState(false);
  const [showServerOpts, setShowServerOpts] = useState(false);
  const [showLayout, setShowLayout] = useState(true);
  const [showLog, setShowLog] = useState(false);
  const [showThemes, setShowThemes] = useState(false);
  const [dismissedFingerprint, setDismissedFingerprint] = useState<string | null>(
    null,
  );
  const [layoutSaveMsg, setLayoutSaveMsg] = useState<string | null>(null);

  const refresh = useCallback(async () => {
    const [st, se, sc, ver] = await Promise.all([
      invoke<CoreStatus>("get_status"),
      invoke<AppSettings>("get_settings"),
      invoke<ServerConfig>("get_server_config"),
      invoke<VersionInfo>("get_version"),
    ]);
    setStatus(st);
    setSettings(se);
    setServerConfig(sc);
    setVersion(ver);
  }, []);

  useEffect(() => {
    refresh().catch((e) => setError(String(e)));
  }, [refresh]);

  useEffect(() => {
    let cancelled = false;
    const unsubs: Array<() => void> = [];

    async function subscribe() {
      try {
        const logUnsub = await listen<string>("deskflow-log", (e) => {
          setLogs((prev) => {
            const next = [...prev, e.payload];
            return next.length > MAX_LOG ? next.slice(-MAX_LOG) : next;
          });
        });
        if (cancelled) {
          logUnsub();
          return;
        }
        unsubs.push(logUnsub);

        const processUnsub = await listen<CoreStatus>("deskflow-process", (e) => {
          setStatus(e.payload);
        });
        if (cancelled) {
          processUnsub();
          return;
        }
        unsubs.push(processUnsub);

        const ipcUnsub = await listen("deskflow-ipc", () => {
          /* status updates also arrive via process events */
        });
        if (cancelled) {
          ipcUnsub();
          return;
        }
        unsubs.push(ipcUnsub);
      } catch (e) {
        if (!cancelled) {
          setError(`event subscribe failed: ${String(e)}`);
        }
      }
    }

    void subscribe();
    return () => {
      cancelled = true;
      unsubs.forEach((u) => u());
    };
  }, []);

  async function saveSettings(next: AppSettings) {
    await invoke("set_settings", { settings: next });
    setSettings(next);
    await refresh();
  }

  async function saveServerConfig(
    next: ServerConfig,
    opts?: { applyRunning?: boolean },
  ) {
    setBusy(true);
    setError(null);
    setLayoutSaveMsg(null);
    try {
      await invoke<string>("set_server_config", { config: next });
      setServerConfig(next);
      const shouldApply =
        opts?.applyRunning !== false &&
        (status?.processState === "started" ||
          status?.processState === "starting");
      if (shouldApply) {
        await invoke("core_restart");
        await refresh();
        setLayoutSaveMsg("Saved and applied");
      } else {
        setLayoutSaveMsg("Layout saved");
      }
    } catch (e) {
      setError(String(e));
      throw e;
    } finally {
      setBusy(false);
    }
  }

  async function setMode(mode: CoreMode) {
    if (!settings) return;
    const next = { ...settings, coreMode: mode };
    await saveSettings(next);
    if (mode === "server") setShowLayout(true);
  }

  async function start() {
    setBusy(true);
    setError(null);
    try {
      if (settings?.coreMode === "server" && serverConfig) {
        const named = serverConfig.screens.filter((s) => s.name).length;
        if (named < 2) {
          const updated = await invoke<ServerConfig>("ensure_minimal_layout", {
            clientName: "client",
          });
          setServerConfig(updated);
        }
      }
      await invoke("core_start");
      await refresh();
    } catch (e) {
      setError(String(e));
    } finally {
      setBusy(false);
    }
  }

  async function stop() {
    setBusy(true);
    setError(null);
    try {
      await invoke("core_stop");
      await refresh();
    } catch (e) {
      setError(String(e));
    } finally {
      setBusy(false);
    }
  }

  async function restart() {
    setBusy(true);
    setError(null);
    try {
      await invoke("core_restart");
      await refresh();
    } catch (e) {
      setError(String(e));
    } finally {
      setBusy(false);
    }
  }

  const fingerprint =
    status?.peerFingerprint && status.peerFingerprint !== dismissedFingerprint
      ? status.peerFingerprint
      : null;

  const running = useMemo(
    () =>
      status?.processState === "started" ||
      status?.processState === "starting",
    [status],
  );

  const connState = status?.connectionState ?? "disconnected";
  const mode = settings?.coreMode ?? "none";
  const isServer = mode === "server";
  const isClient = mode === "client";
  const appName = version?.appName ?? "Deskflow";
  const upper = tc.uppercaseBadges;

  return (
    <div id={`theme-${themeId}`} className={tc.root}>
      <header className={tc.header}>
        <div className="flex min-w-0 flex-1 items-center gap-3 sm:gap-4">
          <div className={tc.logoWrap}>
            <Monitor className={`size-5 ${tc.logoIcon}`} strokeWidth={2} />
          </div>
          <div className="min-w-0">
            <h1 className={tc.title}>{appName}</h1>
            <p className={tc.subtitle}>
              Share one mouse and keyboard across your computers
            </p>
          </div>
        </div>
        <div className="flex max-w-full flex-wrap items-center gap-2 sm:gap-3">
          <div className={tc.badgeConnected}>
            <span className={tc.badgeDotConnected} />
            {badgeText(connectionLabel(connState), upper)}
          </div>
          <div className={tc.badgeRunning}>
            <span className={tc.badgeDotRunning} />
            {badgeText(processLabel(status?.processState), upper)}
          </div>
          {status?.secureSocket && (
            <div className={tc.badgeEncrypted}>
              <Lock className="size-3" strokeWidth={2.5} />
              {badgeText("Encrypted", upper)}
            </div>
          )}
        </div>
      </header>

      <main className={tc.main}>
        <div className={tc.glowA} />
        <div className={tc.glowB} />

        <section className={tc.card}>
          <div className={tc.modeRow}>
            <button
              type="button"
              onClick={() => setMode("server")}
              className={isServer ? tc.modeServerActive : tc.modeServerIdle}
            >
              <div className="relative flex items-start justify-between gap-3">
                <div className="min-w-0 flex-1">
                  <h3 className={isServer ? tc.modeTitleActive : tc.modeTitleIdle}>
                    Server
                    <User
                      className={`size-4 shrink-0 ${isServer ? tc.modeIconActive : tc.modeIconIdle}`}
                    />
                  </h3>
                  <p className={isServer ? tc.modeDescActive : tc.modeDescIdle}>
                    Other computers connect here. Arrange screens below.
                  </p>
                </div>
                <div className={isServer ? tc.radioActive : tc.radioIdle} />
              </div>
            </button>
            <button
              type="button"
              onClick={() => setMode("client")}
              className={isClient ? tc.modeClientActive : tc.modeClientIdle}
            >
              <div className="flex items-start justify-between gap-3">
                <div className="min-w-0 flex-1">
                  <h3 className={isClient ? tc.modeTitleActive : tc.modeTitleIdle}>
                    Client
                    <Monitor
                      className={`size-4 shrink-0 ${isClient ? tc.modeIconActive : tc.modeIconIdle}`}
                    />
                  </h3>
                  <p className={isClient ? tc.modeDescActive : tc.modeDescIdle}>
                    Connect to another computer that is hosting.
                  </p>
                </div>
                <div className={isClient ? tc.radioActive : tc.radioIdle} />
              </div>
            </button>
          </div>

          {isClient && settings && (
            <div className="px-4 py-5 sm:px-6">
              <label className={tc.label}>Server address</label>
              <input
                className={`${tc.input} !max-w-none !w-full !pl-3`}
                value={settings.remoteHost}
                placeholder="e.g. 192.168.1.10 or hostname"
                onChange={(e) =>
                  setSettings({ ...settings, remoteHost: e.target.value })
                }
                onBlur={() => settings && saveSettings(settings)}
              />
              <p className="mt-1.5 text-xs opacity-60">
                IP or hostname of the computer running Deskflow as a server
              </p>
            </div>
          )}

          {isServer && (
            <div className={tc.infoRow}>
              <div>
                <div className={tc.infoLabel}>
                  <Monitor className="size-3" />
                  This Computer
                </div>
                <div className={tc.infoChip}>
                  {settings?.computerName || "—"}
                </div>
              </div>
              <div>
                <div className={tc.infoLabel}>Network Addresses</div>
                <div className="flex flex-wrap gap-2">
                  {(status?.localIps ?? []).length === 0 ? (
                    <span className={tc.infoChip}>—</span>
                  ) : (
                    status!.localIps.map((ip, i) => (
                      <span
                        key={ip}
                        className={i === 0 ? tc.infoChipAccent : tc.infoChip}
                      >
                        {ip}
                      </span>
                    ))
                  )}
                </div>
              </div>
              <div>
                <div className={tc.infoLabel}>Connected Clients</div>
                <div className="flex flex-wrap gap-2">
                  {status?.connectedClients?.length ? (
                    status.connectedClients.map((c) => (
                      <span key={c} className={tc.infoChipClient}>
                        {c}
                      </span>
                    ))
                  ) : (
                    <span className={tc.infoChip}>None yet</span>
                  )}
                </div>
              </div>
            </div>
          )}

          {(error || status?.lastError) && (
            <div role="alert" className={tc.errorBox}>
              {error || status?.lastError}
            </div>
          )}

          <div className={tc.actionsRow}>
            <div className="flex min-w-0 flex-wrap items-center gap-2 sm:gap-3">
              <button
                type="button"
                onClick={start}
                disabled={busy || running}
                className={tc.btnStart}
              >
                <Play className="size-4 shrink-0 fill-current" />
                Start
              </button>
              <button
                type="button"
                onClick={stop}
                disabled={busy || !running}
                className={tc.btnStop}
              >
                <Square className="size-3.5 shrink-0 fill-current" />
                Stop
              </button>
              <button
                type="button"
                onClick={restart}
                disabled={busy}
                className={tc.btnGhost}
              >
                <RotateCcw className="size-4 shrink-0" />
                Restart
              </button>
            </div>
            <div className="flex min-w-0 flex-wrap items-center gap-2 sm:gap-3">
              {isServer && (
                <button
                  type="button"
                  onClick={() => setShowLayout((v) => !v)}
                  className={tc.btnSecondary}
                >
                  {showLayout ? (
                    <EyeOff className="size-4 shrink-0" />
                  ) : (
                    <Eye className="size-4 shrink-0" />
                  )}
                  {showLayout ? "Hide layout" : "Show layout"}
                </button>
              )}
              <button
                type="button"
                onClick={() => setShowThemes(true)}
                className={tc.btnGhost}
              >
                <Palette className="size-4 shrink-0" />
                Theme
              </button>
              <button
                type="button"
                onClick={() => setShowSettings(true)}
                className={tc.btnGhost}
              >
                <Settings2 className="size-4 shrink-0" />
                Preferences
              </button>
            </div>
          </div>
        </section>

        {isServer && showLayout && serverConfig && (
          <section className={tc.layoutCard}>
            <div className={tc.layoutHeader}>
              <div className="flex flex-wrap items-center justify-between gap-3 px-4 py-4 sm:px-6">
                <div className="min-w-0">
                  <h2 className={tc.layoutTitle}>Screen Layout</h2>
                  <p className={tc.layoutSubtitle}>
                    Arrange computers so the cursor can move between them
                  </p>
                </div>
                <button
                  type="button"
                  disabled={busy}
                  onClick={() => saveServerConfig(serverConfig)}
                  className={tc.btnSecondary}
                >
                  {layoutSaveMsg ? "Saved!" : "Save layout"}
                </button>
              </div>
              {layoutSaveMsg && (
                <div className={tc.successBox}>{layoutSaveMsg}</div>
              )}
            </div>
            <LayoutEditor
              config={serverConfig}
              onChange={setServerConfig}
            />
          </section>
        )}
      </main>

      <footer className={tc.footer}>
        <div className="flex min-w-0 flex-wrap items-center gap-x-4 gap-y-2 sm:gap-x-6">
          <button
            type="button"
            className={tc.footerLink}
            onClick={() => setShowClient(true)}
          >
            Client options
          </button>
          <button
            type="button"
            className={tc.footerLink}
            onClick={() => setShowServerOpts(true)}
          >
            Server options
          </button>
          <button
            type="button"
            className={`${tc.footerLink} flex items-center gap-1`}
            onClick={() => setShowLog(true)}
          >
            <Terminal className="size-3 shrink-0" />
            Debug log
            {logs.length > 0 && (
              <span className="opacity-70">({logs.length})</span>
            )}
          </button>
          <button
            type="button"
            className={`${tc.footerLink} flex items-center gap-1`}
            onClick={() => openUrl("https://github.com/deskflow/deskflow")}
          >
            <CircleHelp className="size-3 shrink-0" />
            Help
          </button>
        </div>
        <div className={tc.footerVersion}>{version?.versionId ?? "…"}</div>
      </footer>

      <ThemePicker open={showThemes} onClose={() => setShowThemes(false)} />
      <LogPanel
        open={showLog}
        onClose={() => setShowLog(false)}
        lines={logs}
        onClear={() => setLogs([])}
      />
      <SettingsDialog
        open={showSettings}
        settings={settings}
        onClose={() => setShowSettings(false)}
        onSave={saveSettings}
      />
      <ClientConfigDialog
        open={showClient}
        settings={settings}
        onClose={() => setShowClient(false)}
        onSave={saveSettings}
      />
      <ServerOptionsDialog
        open={showServerOpts}
        config={serverConfig}
        onClose={() => setShowServerOpts(false)}
        onSave={saveServerConfig}
      />
      <TrustPrompt
        fingerprint={fingerprint}
        onDismiss={() =>
          setDismissedFingerprint(status?.peerFingerprint ?? null)
        }
        onTrust={async () => {
          if (!status?.peerFingerprint) return;
          await invoke("trust_fingerprint", {
            fingerprint: status.peerFingerprint,
            asServer: settings?.coreMode === "client",
          });
          setDismissedFingerprint(status.peerFingerprint);
        }}
      />
    </div>
  );
}
