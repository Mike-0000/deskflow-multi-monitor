#Requires -Version 5.1
<#
.SYNOPSIS
  Migrate an existing Deskflow Windows installation to a locally built release.

.DESCRIPTION
  Backs up the current install and settings, stops the Deskflow service, copies
  freshly built binaries (with Qt runtime) into the install directory, and
  restarts the service.

  Settings (Deskflow.conf), TLS certificates, and trusted fingerprints are kept
  in place — they live outside the install folder.

  IMPORTANT: This branch uses protocol 1.9. Upgrade the SERVER machine before
  clients, or clients may be rejected until the server is also on this build.

.PARAMETER InstallPath
  Existing Deskflow installation directory.

.PARAMETER SourceDir
  Directory containing the built release (defaults to repo/build/bin).

.PARAMETER BackupRoot
  Where timestamped backups are stored.

.PARAMETER BuildFirst
  Run build-windows.ps1 before migrating.

.PARAMETER SkipBackup
  Skip backup step (not recommended).

.PARAMETER NoStart
  Do not restart Deskflow after migration.

.PARAMETER Force
  Proceed even when this machine is a client and the server may still be old.

.PARAMETER WhatIf
  Show planned actions without changing anything.

.EXAMPLE
  .\migrate-windows.ps1

.EXAMPLE
  .\migrate-windows.ps1 -BuildFirst -Force
#>
[CmdletBinding(SupportsShouldProcess = $true)]
param(
  [string] $InstallPath = "${env:ProgramFiles}\Deskflow",

  [string] $SourceDir = '',

  [string] $BackupRoot = '',

  [switch] $BuildFirst,
  [switch] $SkipBackup,
  [switch] $NoStart,
  [switch] $Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:PluginDirNames = @(
  'generic', 'iconengines', 'imageformats', 'networkinformation', 'platforms', 'styles', 'tls'
)

$script:ExcludeFilePatterns = @(
  '*test*', 'gtest*', 'gmock*', 'legacytests*', 'vc_redist*'
)

function Write-Step([string] $Message) {
  Write-Host ""
  Write-Host "==> $Message" -ForegroundColor Cyan
}

function Write-Warn([string] $Message) {
  Write-Host "WARNING: $Message" -ForegroundColor Yellow
}

function Resolve-RepoRoot {
  $root = $PSScriptRoot
  if (-not (Test-Path (Join-Path $root 'CMakeLists.txt'))) {
    throw "Run this script from the Deskflow repository root."
  }
  return $root
}

function Test-IsAdmin {
  $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
  $principal = [Security.Principal.WindowsPrincipal]$identity
  return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Get-DeskflowVersion([string] $ExePath) {
  if (-not (Test-Path $ExePath)) {
    return $null
  }
  $output = & $ExePath --version 2>&1 | Out-String
  if ($output -match 'Deskflow:\s+(\S+)') {
    return $Matches[1]
  }
  return $null
}

function Find-SettingsFile {
  $candidates = @(
    (Join-Path $env:APPDATA 'Deskflow\Deskflow.conf'),
    (Join-Path $env:ProgramData 'Deskflow\Deskflow.conf'),
    (Join-Path $InstallPath 'settings\Deskflow.conf')
  )
  foreach ($path in $candidates) {
    if (Test-Path $path) {
      return $path
    }
  }
  return $null
}

function Read-DeskflowRole([string] $SettingsFile) {
  if (-not $SettingsFile -or -not (Test-Path $SettingsFile)) {
    return 'Unknown'
  }

  $coreMode = $null
  $remoteHost = $null
  Get-Content $SettingsFile | ForEach-Object {
    if ($_ -match '^\s*coreMode\s*=\s*(\d+)') { $coreMode = [int]$Matches[1] }
    if ($_ -match '^\s*remoteHost\s*=\s*(.+)') { $remoteHost = $Matches[1].Trim() }
  }

  switch ($coreMode) {
    1 { return 'Client' }
    2 { return 'Server' }
    default { return 'Unknown' }
  }
}

function Get-RemoteHost([string] $SettingsFile) {
  if (-not $SettingsFile -or -not (Test-Path $SettingsFile)) {
    return $null
  }
  foreach ($line in Get-Content $SettingsFile) {
    if ($line -match '^\s*remoteHost\s*=\s*(.+)') {
      return $Matches[1].Trim()
    }
  }
  return $null
}

function Test-HasAdvancedLayout([string] $SettingsFile) {
  if (-not $SettingsFile -or -not (Test-Path $SettingsFile)) {
    return $false
  }
  $content = Get-Content $SettingsFile -Raw
  return ($content -match 'display_layouts') -or ($content -match 'advancedLayout\s*=\s*true')
}

function Stop-Deskflow {
  Write-Step "Stopping Deskflow"

  $service = Get-Service -Name 'Deskflow' -ErrorAction SilentlyContinue
  if ($service -and $service.Status -eq 'Running') {
    if ($PSCmdlet.ShouldProcess('Deskflow service', 'Stop')) {
      Stop-Service -Name 'Deskflow' -Force -ErrorAction Stop
      $service.WaitForStatus('Stopped', [TimeSpan]::FromSeconds(30))
    }
  }

  foreach ($name in @('deskflow', 'deskflow-core', 'deskflow-daemon')) {
    Get-Process -Name $name -ErrorAction SilentlyContinue | ForEach-Object {
      if ($PSCmdlet.ShouldProcess($_.Path, "Stop process $($_.Name)")) {
        Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue
      }
    }
  }

  Start-Sleep -Seconds 2
}

function Start-DeskflowService {
  $service = Get-Service -Name 'Deskflow' -ErrorAction SilentlyContinue
  if (-not $service) {
    Write-Warn "Deskflow Windows service is not installed; start deskflow.exe manually if needed."
    return
  }

  if ($PSCmdlet.ShouldProcess('Deskflow service', 'Start')) {
    Start-Service -Name 'Deskflow'
    $service.WaitForStatus('Running', [TimeSpan]::FromSeconds(30))
  }
}

function New-Backup([string] $Timestamp) {
  Write-Step "Creating backup at $script:BackupDir"

  New-Item -ItemType Directory -Path $script:BackupDir -Force | Out-Null

  if (Test-Path $InstallPath) {
    $installBackup = Join-Path $script:BackupDir 'install'
    if ($PSCmdlet.ShouldProcess($InstallPath, "Copy to $installBackup")) {
      robocopy $InstallPath $installBackup /MIR /R:1 /W:1 /NFL /NDL /NJH /NJS /NC /NS | Out-Null
      if ($LASTEXITCODE -ge 8) {
        throw "Backup of install directory failed (robocopy exit $LASTEXITCODE)."
      }
    }
  }

  $settingsFile = Find-SettingsFile
  if ($settingsFile) {
    $settingsBackup = Join-Path $script:BackupDir 'config'
    New-Item -ItemType Directory -Path $settingsBackup -Force | Out-Null
    Copy-Item $settingsFile (Join-Path $settingsBackup 'Deskflow.conf') -Force
  }

  $tlsDir = Join-Path $env:ProgramData 'Deskflow\tls'
  if (Test-Path $tlsDir) {
    $tlsBackup = Join-Path $script:BackupDir 'tls'
    if ($PSCmdlet.ShouldProcess($tlsDir, "Copy to $tlsBackup")) {
      robocopy $tlsDir $tlsBackup /MIR /R:1 /W:1 /NFL /NDL /NJH /NJS /NC /NS | Out-Null
      if ($LASTEXITCODE -ge 8) {
        throw "Backup of TLS directory failed (robocopy exit $LASTEXITCODE)."
      }
    }
  }

  $manifest = [ordered]@{
    timestamp    = $Timestamp
    installPath  = $InstallPath
    sourceDir    = $script:SourceDir
    settingsFile = $settingsFile
    role         = $script:Role
    remoteHost   = Get-RemoteHost $settingsFile
    oldVersion   = $script:OldVersion
    newVersion   = $script:NewVersion
  }
  $manifest | ConvertTo-Json | Set-Content (Join-Path $script:BackupDir 'manifest.json') -Encoding UTF8
}

function Test-ExcludedFile([string] $FileName) {
  foreach ($pattern in $script:ExcludeFilePatterns) {
    if ($FileName -like $pattern) {
      return $true
    }
  }
  return $false
}

function Prepare-Staging([string] $StagingDir, [string] $QtBin) {
  Write-Step "Preparing staged release in $StagingDir"

  if (Test-Path $StagingDir) {
    Remove-Item $StagingDir -Recurse -Force
  }
  New-Item -ItemType Directory -Path $StagingDir -Force | Out-Null

  $required = @('deskflow.exe', 'deskflow-core.exe', 'deskflow-daemon.exe')
  foreach ($file in $required) {
    $src = Join-Path $script:SourceDir $file
    if (-not (Test-Path $src)) {
      throw "Missing build artifact: $src (run .\build-windows.ps1 first)."
    }
    Copy-Item $src (Join-Path $StagingDir $file)
  }

  Get-ChildItem $script:SourceDir -File | ForEach-Object {
    if (Test-ExcludedFile $_.Name) {
      return
    }
    Copy-Item $_.FullName (Join-Path $StagingDir $_.Name) -Force
  }

  $pluginsSrc = Join-Path $script:SourceDir 'plugins'
  if (Test-Path $pluginsSrc) {
    Copy-Item $pluginsSrc (Join-Path $StagingDir 'plugins') -Recurse -Force
  }
  else {
    $pluginsDest = Join-Path $StagingDir 'plugins'
    New-Item -ItemType Directory -Path $pluginsDest -Force | Out-Null
    foreach ($dir in $script:PluginDirNames) {
      $srcDir = Join-Path $script:SourceDir $dir
      if (Test-Path $srcDir) {
        Copy-Item $srcDir (Join-Path $pluginsDest $dir) -Recurse -Force
      }
    }
  }

  $translationsSrc = Join-Path $script:SourceDir 'translations'
  $translationsDest = Join-Path $StagingDir 'translations'
  if (Test-Path $translationsSrc) {
    Copy-Item $translationsSrc $translationsDest -Recurse -Force
  }
  else {
    New-Item -ItemType Directory -Path $translationsDest -Force | Out-Null
  }

  if (Test-Path $InstallPath) {
    $oldTranslations = Join-Path $InstallPath 'translations'
    if (Test-Path $oldTranslations) {
      Get-ChildItem $oldTranslations -Filter 'deskflow_*.qm' -ErrorAction SilentlyContinue | ForEach-Object {
        Copy-Item $_.FullName (Join-Path $translationsDest $_.Name) -Force
      }
    }
  }

  foreach ($licenseName in @('LICENSE', 'LICENSE_EXCEPTION')) {
    $repoLicense = Join-Path $script:RepoRoot $licenseName
    if (Test-Path $repoLicense) {
      Copy-Item $repoLicense (Join-Path $StagingDir $licenseName) -Force
    }
    elseif (Test-Path (Join-Path $InstallPath $licenseName)) {
      Copy-Item (Join-Path $InstallPath $licenseName) (Join-Path $StagingDir $licenseName) -Force
    }
  }

  $windeployqt = Join-Path $QtBin 'windeployqt.exe'
  if (-not (Test-Path $windeployqt)) {
    throw "windeployqt not found at $windeployqt"
  }

  $deskflowExe = Join-Path $StagingDir 'deskflow.exe'
  if (-not (Test-Path $deskflowExe)) {
    Write-Warn "Skipping windeployqt because staged deskflow.exe was not created (WhatIf mode?)."
    return
  }

  if ($PSCmdlet.ShouldProcess($deskflowExe, 'Run windeployqt')) {
    $prevEap = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
      & $windeployqt `
        --no-translations `
        --no-compiler-runtime `
        --no-system-d3d-compiler `
        --no-opengl-sw `
        --plugindir (Join-Path $StagingDir 'plugins') `
        --dir $StagingDir `
        $deskflowExe 2>&1 | ForEach-Object {
          if ($_ -is [System.Management.Automation.ErrorRecord]) {
            Write-Host $_.Exception.Message
          }
          else {
            Write-Host $_
          }
        }
    }
    finally {
      $ErrorActionPreference = $prevEap
    }

    if ($LASTEXITCODE -ne 0) {
      throw "windeployqt failed while preparing staged release (exit $LASTEXITCODE)."
    }
  }

  Remove-Item (Join-Path $StagingDir 'plugins\plugins') -Recurse -Force -ErrorAction SilentlyContinue
}

function Install-StagedRelease([string] $StagingDir) {
  Write-Step "Installing into $InstallPath"

  if (-not (Test-Path $InstallPath)) {
    New-Item -ItemType Directory -Path $InstallPath -Force | Out-Null
  }

  if ($PSCmdlet.ShouldProcess($InstallPath, 'Deploy staged Deskflow release')) {
    robocopy $StagingDir $InstallPath /MIR /R:2 /W:2 /NFL /NDL /NJH /NJS /NC /NS | Out-Null
    if ($LASTEXITCODE -ge 8) {
      throw "Install failed (robocopy exit $LASTEXITCODE)."
    }
  }
}

function Find-QtBin([string] $RepoRoot) {
  $qtRoot = Join-Path $RepoRoot '.qt\6.10.3\msvc2022_64\bin'
  if (Test-Path (Join-Path $qtRoot 'windeployqt.exe')) {
    return $qtRoot
  }

  $windeployqt = Get-Command windeployqt.exe -ErrorAction SilentlyContinue
  if ($windeployqt) {
    return Split-Path $windeployqt.Source -Parent
  }

  throw "Could not find windeployqt. Build Qt locally with build-windows.ps1 or add Qt bin to PATH."
}

# --- main ---

$script:RepoRoot = Resolve-RepoRoot

if (-not $SourceDir) {
  $SourceDir = Join-Path $script:RepoRoot 'build\bin'
}
$script:SourceDir = (Resolve-Path $SourceDir -ErrorAction Stop).Path

if (-not $BackupRoot) {
  $BackupRoot = Join-Path $script:RepoRoot '.migration-backup'
}
if (-not (Test-Path $BackupRoot)) {
  if ($PSCmdlet.ShouldProcess($BackupRoot, 'Create backup root directory')) {
    New-Item -ItemType Directory -Path $BackupRoot -Force | Out-Null
  }
}
if (Test-Path $BackupRoot) {
  $BackupRoot = (Resolve-Path -LiteralPath $BackupRoot).Path
}
else {
  $BackupRoot = [System.IO.Path]::GetFullPath($BackupRoot)
}

$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$script:BackupDir = Join-Path $BackupRoot $timestamp

$settingsFile = Find-SettingsFile
$script:Role = Read-DeskflowRole $settingsFile
$remoteHost = Get-RemoteHost $settingsFile
$hasAdvanced = Test-HasAdvancedLayout $settingsFile

Write-Step "Deskflow migration preflight"
Write-Host "  Role:           $script:Role"
Write-Host "  Settings:       $(if ($settingsFile) { $settingsFile } else { '(not found)' })"
Write-Host "  Install path:   $InstallPath"
Write-Host "  Source build:   $script:SourceDir"
Write-Host "  Remote server:  $(if ($remoteHost) { $remoteHost } else { '(none configured)' })"
Write-Host "  Advanced layout: $(if ($hasAdvanced) { 'enabled in config' } else { 'not configured (legacy links)' })"

if ($BuildFirst) {
  Write-Step "Building latest release"
  $buildScript = Join-Path $script:RepoRoot 'build-windows.ps1'
  if (-not (Test-Path $buildScript)) {
    throw "Missing build script: $buildScript"
  }
  & $buildScript -SkipQtInstall -SkipVcpkgBootstrap
  if ($LASTEXITCODE -ne 0) {
    throw "build-windows.ps1 failed."
  }
}

$oldExe = Join-Path $InstallPath 'deskflow.exe'
$newExe = Join-Path $script:SourceDir 'deskflow.exe'
$script:OldVersion = Get-DeskflowVersion $oldExe
$script:NewVersion = Get-DeskflowVersion $newExe

Write-Host "  Installed ver:  $(if ($script:OldVersion) { $script:OldVersion } else { '(no prior install)' })"
Write-Host "  Built ver:      $(if ($script:NewVersion) { $script:NewVersion } else { '(build missing — run build-windows.ps1)' })"

if (-not $script:NewVersion) {
  throw "Built deskflow.exe was not found or could not report a version."
}

if ($script:Role -eq 'Client' -and -not $Force -and -not $WhatIfPreference) {
  Write-Warn @"
This machine is configured as a CLIENT. This build uses protocol 1.9.
Upgrade the SERVER ($remoteHost) to this build BEFORE migrating clients,
or the server may reject the connection.

Re-run with -Force to migrate this machine anyway.
"@
  throw "Aborted pending server upgrade. Use -Force to override."
}

if ($script:Role -eq 'Client' -and -not $Force -and $WhatIfPreference) {
  Write-Warn "Client migration would require -Force because protocol 1.9 needs a matching server."
}

if (-not (Test-IsAdmin) -and -not $WhatIfPreference) {
  throw "Administrator privileges are required to update '$InstallPath' and manage the Deskflow service."
}

if (-not (Test-IsAdmin) -and $WhatIfPreference) {
  Write-Warn "Not running as Administrator; remaining steps are preview only."
}

if ($WhatIfPreference) {
  Write-Host ""
  Write-Host "WhatIf preview complete. To apply:" -ForegroundColor Green
  Write-Host "  1. Upgrade the server at $remoteHost first (if this is a client)."
  Write-Host "  2. Open PowerShell as Administrator."
  Write-Host "  3. cd `"$script:RepoRoot`""
  if ($script:Role -eq 'Client') {
    Write-Host "  4. .\migrate-windows.ps1 -Force"
  }
  else {
    Write-Host "  4. .\migrate-windows.ps1"
  }
  return
}

$wasRunning = $false
$service = Get-Service -Name 'Deskflow' -ErrorAction SilentlyContinue
if ($service -and $service.Status -eq 'Running') {
  $wasRunning = $true
}

$stagingDir = Join-Path $script:RepoRoot ".migration-staging-$timestamp"
$qtBin = Find-QtBin $script:RepoRoot

try {
  if (-not $SkipBackup) {
    New-Backup $timestamp
  }
  else {
    Write-Warn "Skipping backup (-SkipBackup)."
  }

  Stop-Deskflow
  Prepare-Staging $stagingDir $qtBin
  Install-StagedRelease $stagingDir

  $installedVersion = Get-DeskflowVersion (Join-Path $InstallPath 'deskflow.exe')
  Write-Host ""
  Write-Host "Migration complete." -ForegroundColor Green
  Write-Host "  Version:  $installedVersion"
  Write-Host "  Backup:   $script:BackupDir"
  Write-Host "  Install:  $InstallPath"

  if ($script:Role -eq 'Server') {
    Write-Host ""
    Write-Host "Next steps (server):"
    Write-Host "  1. Open Deskflow GUI -> Server configuration."
    Write-Host "  2. Existing screen links still work unchanged."
    Write-Host "  3. Optional: Advanced Layout tab -> Import Local Displays to enable per-monitor routing."
    Write-Host "  4. Upgrade each client machine with .\migrate-windows.ps1 -Force"
  }
  elseif ($script:Role -eq 'Client') {
    Write-Host ""
    Write-Host "Next steps (client):"
    Write-Host "  1. Ensure the server at $remoteHost is already on this build."
    Write-Host "  2. Confirm connection from the Deskflow GUI or tray icon."
    Write-Host "  3. No config changes are required unless the server enables advanced layout."
  }

  if (-not $hasAdvanced) {
    Write-Host ""
    Write-Host "Your existing links-based layout is unchanged. Advanced monitor layout is opt-in."
  }

  if ($wasRunning -and -not $NoStart) {
    Start-DeskflowService
    Write-Host "  Service:  restarted"
  }
  elseif ($NoStart) {
    Write-Host "  Service:  left stopped (-NoStart)"
  }
}
finally {
  Remove-Item $stagingDir -Recurse -Force -ErrorAction SilentlyContinue
}
