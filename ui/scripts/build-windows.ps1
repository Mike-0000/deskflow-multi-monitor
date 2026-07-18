#Requires -Version 5.1
<#
.SYNOPSIS
  Build the Deskflow Tauri UI and stage it next to C++ binaries.

.DESCRIPTION
  Generates version metadata, builds the Tauri app in release mode, then copies
  Deskflow.exe to build/bin/deskflow-ui.exe beside deskflow-core.exe.

.PARAMETER SkipNpmInstall
  Skip npm install.

.PARAMETER CoreSource
  Optional path to deskflow-core.exe to copy beside the UI for local runs.
#>
[CmdletBinding()]
param(
  [switch] $SkipNpmInstall,
  [string] $CoreSource = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$uiRoot = Split-Path $PSScriptRoot -Parent
$repoRoot = Split-Path $uiRoot -Parent
$binDir = Join-Path $repoRoot 'build\bin'

Set-Location $uiRoot

if (-not $SkipNpmInstall) {
  Write-Host "==> npm install" -ForegroundColor Cyan
  npm install
}

Write-Host "==> generate version.json" -ForegroundColor Cyan
npm run gen:version

Write-Host "==> tauri build" -ForegroundColor Cyan
$prevEap = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
npm run tauri build
$tauriExit = $LASTEXITCODE
$ErrorActionPreference = $prevEap
if ($tauriExit -ne 0) {
  throw "tauri build failed (exit $tauriExit)"
}

$candidates = @(
  (Join-Path $uiRoot 'src-tauri\target\release\deskflow-ui.exe'),
  (Join-Path $uiRoot 'src-tauri\target\release\Deskflow.exe'),
  (Join-Path $uiRoot 'src-tauri\target\release\ui.exe')
)

$built = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $built) {
  throw "Could not find built Tauri executable under src-tauri/target/release"
}

New-Item -ItemType Directory -Path $binDir -Force | Out-Null
$dest = Join-Path $binDir 'deskflow-ui.exe'
Copy-Item $built $dest -Force
Write-Host "==> staged $dest" -ForegroundColor Green

if (-not $CoreSource) {
  $defaultCore = Join-Path $binDir 'deskflow-core.exe'
  if (Test-Path $defaultCore) {
    $CoreSource = $defaultCore
  }
}

if ($CoreSource -and (Test-Path $CoreSource)) {
  # Ensure core sits beside the UI binary for desktop process discovery.
  $coreBesideUi = Join-Path (Split-Path $built -Parent) 'deskflow-core.exe'
  Copy-Item $CoreSource $coreBesideUi -Force
  Write-Host "==> copied core beside Tauri release binary" -ForegroundColor Green
}

Write-Host "Done. Run: $dest" -ForegroundColor Green
