# Deskflow UI (Tauri 2 + React)

Parallel replacement for the Qt Widgets GUI. Talks to the existing
`deskflow-core` / `deskflow-daemon` processes over the same local-socket IPC.

## Stack

- Tauri 2 + Rust (process lifecycle, IPC, settings, tray)
- React + TypeScript + Vite + Tailwind
- Compatible INI / `deskflow-server.conf` with the Qt GUI

## Prerequisites

- Node.js 20+
- Rust (stable)
- WebView2 (Windows)
- Built `deskflow-core` (and optionally `deskflow-daemon`) from the CMake project

## Develop

```powershell
# From repo root, build core first if needed
.\build-windows.ps1

cd ui
npm install
npm run gen:version

# Point at the CMake core binary during dev
$env:DESKFLOW_CORE_PATH = "$PWD\..\build\bin\deskflow-core.exe"
npm run tauri:dev
```

## Release build (Windows)

```powershell
cd ui
.\scripts\build-windows.ps1
# Produces build\bin\deskflow-ui.exe
```

## Migrate (replace Qt GUI with Tauri)

Defaults to Tauri cutover (`-Force -UseTauriUi -BuildTauriUi -BuildFirst`).
Requires Administrator.

```powershell
.\migrate-windows.ps1
```

WebView2 Runtime is required on the target machine.

Legacy Qt migrate: `.\migrate-windows.ps1 -UseTauriUi:$false -BuildTauriUi:$false`

## Architecture

| Layer | Responsibility |
|---|---|
| React | Screens, layout editor, dialogs, log view |
| Rust commands | `core_start` / `core_stop`, settings, server config |
| IPC | `hello=<version>+<sha>` newline protocol on `deskflow-core` / `deskflow-daemon` |
| C++ core | Unchanged input sharing engine |

Version metadata is generated into `src-tauri/version.json` so the IPC handshake
matches the built core.
