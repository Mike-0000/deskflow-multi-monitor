import { invoke } from "@tauri-apps/api/core";
import { Download, Monitor, RefreshCw } from "lucide-react";
import { useEffect, useMemo, useState } from "react";
import { EDGE_ABUT_TOLERANCE, layoutH, layoutW } from "../lib/layoutSnap";
import { useTheme } from "../themes";
import type { DisplayRect, ScreenCell, ServerConfig } from "../types";
import { LayoutCanvas, type MonitorRef } from "./LayoutCanvas";

export function LayoutEditor({
  config,
  onChange,
}: {
  config: ServerConfig;
  onChange: (next: ServerConfig) => void;
}) {
  const { tc } = useTheme();
  const [selected, setSelected] = useState<number | null>(null);
  const [newName, setNewName] = useState("");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const namedScreens = useMemo(
    () => config.screens.filter((s) => s.name),
    [config.screens],
  );

  const machineNames = useMemo(() => {
    const fromScreens = namedScreens.map((s) => s.name);
    const fromLayout = config.workspaceLayout.machines.map((m) => m.name);
    return Array.from(new Set([...fromScreens, ...fromLayout]));
  }, [namedScreens, config.workspaceLayout.machines]);

  const [selectedMachine, setSelectedMachine] = useState(
    () => machineNames[0] ?? "",
  );
  const [monitorSel, setMonitorSel] = useState<MonitorRef | null>(null);

  useEffect(() => {
    if (!selectedMachine && machineNames[0]) {
      setSelectedMachine(machineNames[0]);
    } else if (
      selectedMachine &&
      machineNames.length > 0 &&
      !machineNames.includes(selectedMachine)
    ) {
      setSelectedMachine(machineNames[0]);
    }
  }, [machineNames, selectedMachine]);

  const grid = useMemo(() => {
    return config.screens.map((screen, index) => ({ ...screen, index }));
  }, [config.screens]);

  const selectedScreen = selected != null ? config.screens[selected] : null;
  const advanced = config.workspaceLayout.enabled;

  function updateScreen(index: number, patch: Partial<ScreenCell>) {
    const screens = config.screens.map((s, i) =>
      i === index ? { ...s, ...patch } : { ...s },
    );
    if (patch.isServer) {
      screens.forEach((s, i) => {
        if (i !== index) s.isServer = false;
      });
    }
    onChange({ ...config, screens });
  }

  function addNamedScreen() {
    const name = newName.trim();
    if (!name) return;
    const empty = config.screens.findIndex((s) => !s.name);
    if (empty < 0) return;
    updateScreen(empty, { name });
    setNewName("");
    setSelected(empty);
  }

  async function enableAdvanced(enabled: boolean) {
    setError(null);
    const previous = config;
    let next: ServerConfig = {
      ...config,
      workspaceLayout: {
        ...config.workspaceLayout,
        enabled,
        version: 2,
      },
    };
    onChange(next);
    try {
      setBusy(true);
      await invoke("set_server_config", { config: next });
      if (enabled) {
        next = await invoke<ServerConfig>("sync_layout_machines");
        if (!next.workspaceLayout.enabled) {
          next = {
            ...next,
            workspaceLayout: {
              ...next.workspaceLayout,
              enabled: true,
              version: 2,
            },
          };
          await invoke("set_server_config", { config: next });
        }
        onChange(next);
        const names = next.screens.filter((s) => s.name).map((s) => s.name);
        if (names[0]) setSelectedMachine(names[0]);
      }
    } catch (e) {
      onChange(previous);
      setError(String(e));
    } finally {
      setBusy(false);
    }
  }

  function patchWorkspace(
    machines: ServerConfig["workspaceLayout"]["machines"],
  ) {
    onChange({
      ...config,
      workspaceLayout: {
        ...config.workspaceLayout,
        version: 2,
        machines,
      },
    });
  }

  function moveMonitor(ref: MonitorRef, worldX: number, worldY: number) {
    const machines = config.workspaceLayout.machines.map((m, mi) => {
      if (mi !== ref.machineIndex) return m;
      return {
        ...m,
        monitors: m.monitors.map((mon, di) =>
          di === ref.monitorIndex
            ? { ...mon, worldX, worldY, needsPlacement: false }
            : mon,
        ),
      };
    });
    patchWorkspace(machines);
  }

  function updateSelectedMonitor(patch: Partial<DisplayRect>) {
    if (!monitorSel) return;
    const machines = config.workspaceLayout.machines.map((m, mi) => {
      if (mi !== monitorSel.machineIndex) return m;
      return {
        ...m,
        monitors: m.monitors.map((mon, di) =>
          di === monitorSel.monitorIndex
            ? { ...mon, ...patch, needsPlacement: false }
            : mon,
        ),
      };
    });
    patchWorkspace(machines);
  }

  async function importLocal() {
    if (!selectedMachine) {
      setError("Select a machine first (usually this computer).");
      return;
    }
    setBusy(true);
    setError(null);
    try {
      const next = await invoke<ServerConfig>("import_local_displays", {
        machineName: selectedMachine,
      });
      onChange(next);
    } catch (e) {
      setError(String(e));
    } finally {
      setBusy(false);
    }
  }

  async function syncMachines() {
    try {
      setBusy(true);
      const next = await invoke<ServerConfig>("sync_layout_machines");
      onChange(next);
    } catch (e) {
      setError(String(e));
    } finally {
      setBusy(false);
    }
  }

  function removeSelectedMonitor() {
    if (!monitorSel) return;
    const machines = config.workspaceLayout.machines.map((m, mi) => {
      if (mi !== monitorSel.machineIndex) return m;
      return {
        ...m,
        monitors: m.monitors.filter((_, di) => di !== monitorSel.monitorIndex),
      };
    });
    patchWorkspace(machines);
    setMonitorSel(null);
  }

  const selectedMonitor =
    monitorSel != null
      ? config.workspaceLayout.machines[monitorSel.machineIndex]?.monitors[
          monitorSel.monitorIndex
        ]
      : null;

  return (
    <div className="flex flex-1 flex-col">
      <div className={tc.layoutToolbar}>
        <div className="flex flex-wrap items-end gap-5">
          <div>
            <label className={tc.label}>Add Computer</label>
            <div className="flex items-center gap-2">
              <div className="relative">
                <Monitor className="pointer-events-none absolute left-3 top-1/2 size-3.5 -translate-y-1/2 opacity-50" />
                <input
                  type="text"
                  placeholder="Computer name or IP"
                  value={newName}
                  onChange={(e) => setNewName(e.target.value)}
                  onKeyDown={(e) => e.key === "Enter" && addNamedScreen()}
                  className={tc.input}
                />
              </div>
              <button
                type="button"
                onClick={addNamedScreen}
                className={tc.btnSecondary}
              >
                Add
              </button>
            </div>
          </div>
          <div className={tc.divider} />
          <div className="flex items-center gap-3 pb-2">
            <label className="relative inline-flex cursor-pointer items-center">
              <input
                type="checkbox"
                className="switch-peer sr-only"
                checked={advanced}
                disabled={busy}
                onChange={(e) => enableAdvanced(e.target.checked)}
              />
              <div className={tc.toggleTrack}>
                <div className={tc.toggleDot} />
              </div>
              <span className="ml-3 select-none text-sm font-medium opacity-80">
                Advanced monitor layout
              </span>
            </label>
          </div>
        </div>

        {advanced && (
          <div className="flex flex-wrap items-end gap-4">
            <div className="flex flex-col">
              <label className={tc.label}>Editing Machine</label>
              <div className="relative">
                <select
                  value={selectedMachine}
                  onChange={(e) => {
                    setSelectedMachine(e.target.value);
                    setMonitorSel(null);
                  }}
                  className={tc.select}
                >
                  {machineNames.length === 0 && (
                    <option value="">Add screens first</option>
                  )}
                  {machineNames.map((name) => (
                    <option key={name} value={name}>
                      {name}
                    </option>
                  ))}
                </select>
              </div>
            </div>
            <div className="flex gap-2">
              <button
                type="button"
                disabled={busy || !selectedMachine}
                onClick={importLocal}
                className={tc.btnSecondary}
                title="Import local displays"
              >
                <Download className="size-3.5" />
                Import
              </button>
              <button
                type="button"
                disabled={busy}
                onClick={syncMachines}
                className={tc.btnSecondary}
                title="Sync from screen list"
              >
                <RefreshCw className="size-3.5" />
                Sync
              </button>
            </div>
          </div>
        )}
      </div>

      <div className={tc.layoutHint}>
        {advanced
          ? `Place each computer's monitors in shared world space so edges touch (≤${EDGE_ABUT_TOLERANCE} layout units). When advanced layout is on, legacy screen links are ignored.`
          : "Add computers below, or enable advanced monitor layout for pixel-accurate placement."}
      </div>

      {error && <div className={tc.errorBox}>{error}</div>}

      {!advanced && (
        <div className="space-y-4 p-6">
          <div
            className="grid gap-2"
            style={{
              gridTemplateColumns: `repeat(${config.columns}, minmax(0, 1fr))`,
            }}
          >
            {grid.map((cell) => (
              <button
                key={cell.index}
                type="button"
                onClick={() => setSelected(cell.index)}
                className={`min-h-[72px] border px-2 py-3 text-left transition ${
                  selected === cell.index
                    ? tc.modeServerActive
                    : cell.name
                      ? tc.btnSecondary
                      : tc.btnGhost
                }`}
              >
                <div className="truncate text-sm font-medium">
                  {cell.name || "Empty"}
                </div>
                {cell.isServer && (
                  <div className="mt-1 text-[10px] uppercase tracking-wide opacity-70">
                    Server
                  </div>
                )}
              </button>
            ))}
          </div>

          {selectedScreen && selected != null && (
            <div className="grid gap-3 sm:grid-cols-2">
              <div>
                <label className={tc.label}>Screen name</label>
                <input
                  className={`${tc.input} !w-full !pl-3`}
                  value={selectedScreen.name}
                  onChange={(e) =>
                    updateScreen(selected, { name: e.target.value })
                  }
                />
              </div>
              <div>
                <label className={tc.label}>Aliases (comma-separated)</label>
                <input
                  className={`${tc.input} !w-full !pl-3`}
                  value={selectedScreen.aliases.join(", ")}
                  onChange={(e) =>
                    updateScreen(selected, {
                      aliases: e.target.value
                        .split(",")
                        .map((s) => s.trim())
                        .filter(Boolean),
                    })
                  }
                />
              </div>
              <label className="flex items-center gap-2 text-sm">
                <input
                  type="checkbox"
                  checked={selectedScreen.isServer}
                  onChange={(e) =>
                    updateScreen(selected, { isServer: e.target.checked })
                  }
                />
                This is the server screen
              </label>
              <div>
                <button
                  type="button"
                  className={tc.btnSecondary}
                  onClick={() =>
                    updateScreen(selected, {
                      name: "",
                      aliases: [],
                      isServer: false,
                    })
                  }
                >
                  Clear cell
                </button>
              </div>
            </div>
          )}
        </div>
      )}

      {advanced && (
        <>
          <div className={tc.layoutCanvas}>
            <LayoutCanvas
              layout={config.workspaceLayout}
              selectedMachine={selectedMachine}
              selection={monitorSel}
              onSelect={setMonitorSel}
              onMoveMonitor={moveMonitor}
            />
            <div className="pointer-events-none absolute bottom-0 left-0 right-0 flex justify-center bg-gradient-to-t from-black/50 to-transparent px-6 pb-3 pt-12">
              <p className="flex gap-4 rounded-full border border-white/5 bg-black/40 px-4 py-1.5 text-xs text-gray-400 backdrop-blur-sm">
                <span>
                  <strong className="text-gray-300">Drag</strong> monitors to
                  arrange
                </span>
                <span>
                  <strong className="text-gray-300">Scroll</strong> to zoom
                </span>
                <span>
                  <strong className="text-gray-300">Pan</strong> background
                </span>
              </p>
            </div>
          </div>

          {selectedMonitor && monitorSel && (
            <div className="grid gap-3 border-t border-white/5 p-4 sm:grid-cols-4">
              <div className="sm:col-span-2">
                <div className={tc.label}>Selected</div>
                <div className="text-sm font-medium">
                  {
                    config.workspaceLayout.machines[monitorSel.machineIndex]
                      ?.name
                  }{" "}
                  / {selectedMonitor.name || selectedMonitor.id}
                </div>
              </div>
              <label className={tc.label}>
                World X
                <input
                  type="number"
                  className={`${tc.input} !w-full !pl-3`}
                  value={selectedMonitor.worldX}
                  onChange={(e) =>
                    updateSelectedMonitor({ worldX: Number(e.target.value) })
                  }
                />
              </label>
              <label className={tc.label}>
                World Y
                <input
                  type="number"
                  className={`${tc.input} !w-full !pl-3`}
                  value={selectedMonitor.worldY}
                  onChange={(e) =>
                    updateSelectedMonitor({ worldY: Number(e.target.value) })
                  }
                />
              </label>
              <label className={tc.label}>
                Layout W
                <input
                  type="number"
                  className={`${tc.input} !w-full !pl-3`}
                  value={layoutW(selectedMonitor)}
                  onChange={(e) =>
                    updateSelectedMonitor({
                      layoutWidth: Number(e.target.value),
                    })
                  }
                />
              </label>
              <label className={tc.label}>
                Layout H
                <input
                  type="number"
                  className={`${tc.input} !w-full !pl-3`}
                  value={layoutH(selectedMonitor)}
                  onChange={(e) =>
                    updateSelectedMonitor({
                      layoutHeight: Number(e.target.value),
                    })
                  }
                />
              </label>
              <div className="flex items-end sm:col-span-2">
                <button
                  type="button"
                  className={tc.btnStop}
                  onClick={removeSelectedMonitor}
                >
                  Remove monitor
                </button>
              </div>
            </div>
          )}
        </>
      )}
    </div>
  );
}
