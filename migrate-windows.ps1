#Requires -Version 5.1
<#
.SYNOPSIS
  Migrate an existing Deskflow Windows installation to a locally built release.

.DESCRIPTION
  Backs up the current install and settings, stops the Deskflow service, copies
  freshly built binaries into the install directory, and restarts the service.

  Settings (Deskflow.conf), TLS certificates, and trusted fingerprints are kept
  in place — they live outside the install folder.

  IMPORTANT: This branch uses protocol 1.9. Upgrade the SERVER machine before
  clients, or clients may be rejected until the server is also on this build.

  GUI modes:
    Default          - Tauri cutover: build C++ + Tauri UI, replace deskflow.exe,
                       ship core/daemon, and deploy Qt runtime required by deskflow-core.
    -UseTauriUi:$false -BuildTauriUi:$false
                     - Legacy Qt deskflow.exe + windeployqt migrate.

.PARAMETER InstallPath
  Existing Deskflow installation directory.

.PARAMETER SourceDir
  Directory containing the built release (defaults to repo/build/bin).

.PARAMETER BackupRoot
  Where timestamped backups are stored.

.PARAMETER BuildFirst
  Run build-windows.ps1 before migrating (C++ core/daemon). Default: on.
  Disable with -BuildFirst:$false.

.PARAMETER SkipBackup
  Skip backup step (not recommended).

.PARAMETER NoStart
  Do not restart Deskflow after migration.

.PARAMETER Force
  Proceed even when this machine is a client and the server may still be old.
  Default: on. Disable with -Force:$false to keep the client safety check.

.PARAMETER UseTauriUi
  Cut over the installed GUI to Tauri: stage deskflow-ui.exe as deskflow.exe,
  deploy deskflow-core.exe and deskflow-daemon.exe, and run windeployqt on
  deskflow-core (core still links Qt). Default: on.
  Disable with -UseTauriUi:$false for legacy Qt GUI migrate.

.PARAMETER BuildTauriUi
  Build the Tauri UI via ui/scripts/build-windows.ps1 before staging.
  Implies -UseTauriUi when enabled. Default: on.
  Disable with -BuildTauriUi:$false.

.PARAMETER IncludeTauriUi
  Deprecated alias for side-by-side: keep Qt deskflow.exe and also copy
  deskflow-ui.exe. Prefer the default Tauri cutover.

.PARAMETER WhatIf
  Show planned actions without changing anything.

.EXAMPLE
  .\migrate-windows.ps1

.EXAMPLE
  .\migrate-windows.ps1 -WhatIf

.EXAMPLE
  # Legacy Qt GUI migrate
  .\migrate-windows.ps1 -UseTauriUi:$false -BuildTauriUi:$false
#>
[CmdletBinding(SupportsShouldProcess = $true)]
param(
  [string] $InstallPath = "${env:ProgramFiles}\Deskflow",

  [string] $SourceDir = '',

  [string] $BackupRoot = '',

  [switch] $BuildFirst = $true,
  [switch] $SkipBackup,
  [switch] $NoStart,
  [switch] $Force = $true,
  [switch] $UseTauriUi = $true,
  [switch] $BuildTauriUi = $true,
  [switch] $IncludeTauriUi
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:PluginDirNames = @(
  'generic', 'iconengines', 'imageformats', 'networkinformation', 'platforms', 'styles', 'tls'
)

$script:ExcludeFilePatterns = @(
  '*test*', 'gtest*', 'gmock*', 'legacytests*', 'vc_redist*', 'deskflow-ui.exe'
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

  # Qt / core binaries print "Deskflow: <ver>"
  try {
    $output = & $ExePath --version 2>&1 | Out-String
    if ($output -match 'Deskflow:\s+(\S+)') {
      return $Matches[1]
    }
  } catch {
    # Tauri GUI may not support --version; fall through.
  }

  # File version resource (works for many PE builds)
  try {
    $vi = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($ExePath)
    if ($vi.ProductVersion) { return $vi.ProductVersion }
    if ($vi.FileVersion) { return $vi.FileVersion }
  } catch {}

  return $null
}

function Get-TauriVersionId([string] $RepoRoot) {
  $vj = Join-Path $RepoRoot 'ui\src-tauri\version.json'
  if (Test-Path $vj) {
    try {
      $json = Get-Content $vj -Raw | ConvertFrom-Json
      if ($json.versionId) { return [string]$json.versionId }
      if ($json.version) { return [string]$json.version }
    } catch {}
  }
  return 'tauri-ui'
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
  Get-Content $SettingsFile | ForEach-Object {
    if ($_ -match '^\s*coreMode\s*=\s*(\d+)') { $coreMode = [int]$Matches[1] }
    if ($_ -match '^\s*core/coreMode\s*=\s*(\d+)') { $coreMode = [int]$Matches[1] }
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
    if ($line -match '^\s*remoteHost\s*=\s*(.+)' -or $line -match '^\s*client/remoteHost\s*=\s*(.+)') {
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

function Test-WebView2Installed {
  $keys = @(
    'HKLM:\SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}',
    'HKLM:\SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}',
    'HKCU:\SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}'
  )
  foreach ($key in $keys) {
    $prop = Get-ItemProperty -Path $key -Name 'pv' -ErrorAction SilentlyContinue
    if ($prop -and $prop.pv -and $prop.pv -ne '0.0.0.0') {
      return $true
    }
  }
  return $false
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

  foreach ($name in @('deskflow', 'deskflow-ui', 'Deskflow', 'deskflow-core', 'deskflow-daemon')) {
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

function Get-InteractiveUserName {
  $name = (Get-CimInstance Win32_ComputerSystem -ErrorAction SilentlyContinue).UserName
  if ($name) { return $name }
  return $null
}

function Get-DeskflowGuiProcess {
  Get-Process -Name 'deskflow','deskflow-ui' -ErrorAction SilentlyContinue |
    Where-Object { $_.SessionId -gt 0 -and $_.Path -and ($_.Path -like '*\Deskflow\*') }
}

function Start-DeskflowGuiForUser {
  param(
    [Parameter(Mandatory = $true)][string] $GuiPath,
    [Parameter(Mandatory = $true)][string] $UserName,
    [switch] $RegisterLogon
  )

  if (-not (Test-Path $GuiPath)) {
    Write-Warn "GUI binary missing: $GuiPath"
    return
  }

  if (-not $PSCmdlet.ShouldProcess($GuiPath, "Launch tray GUI for $UserName")) {
    return
  }

  $workDir = Split-Path -Parent $GuiPath

  Get-Process -Name 'deskflow','deskflow-ui' -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -and ($_.Path -like '*\Deskflow\*') } |
    ForEach-Object {
      try { Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue } catch {}
    }
  Start-Sleep -Milliseconds 500

  $prevEap = $ErrorActionPreference
  $ErrorActionPreference = 'Continue'
  try {
    $sessionId = [System.Diagnostics.Process]::GetCurrentProcess().SessionId

    if ($sessionId -gt 0) {
      try {
        Start-Process -FilePath $GuiPath -WorkingDirectory $workDir | Out-Null
        Write-Host "Started deskflow.exe in session $sessionId (direct)." -ForegroundColor Cyan
      }
      catch {
        Write-Warn "Direct Start-Process failed: $($_.Exception.Message)"
      }
    }

    if (-not (Get-DeskflowGuiProcess)) {
      $bootstrap = 'DeskflowGuiBootstrap'
      Unregister-ScheduledTask -TaskName $bootstrap -Confirm:$false -ErrorAction SilentlyContinue
      $action = New-ScheduledTaskAction -Execute $GuiPath -WorkingDirectory $workDir
      $principal = New-ScheduledTaskPrincipal -UserId $UserName -LogonType Interactive -RunLevel Limited
      $trigger = New-ScheduledTaskTrigger -Once -At (Get-Date).AddMinutes(-1)
      $settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -StartWhenAvailable
      try {
        Register-ScheduledTask -TaskName $bootstrap -Action $action -Principal $principal -Trigger $trigger -Settings $settings -Force | Out-Null
        Start-ScheduledTask -TaskName $bootstrap -ErrorAction SilentlyContinue
        Write-Host "Started deskflow.exe via scheduled task for $UserName." -ForegroundColor Cyan
      }
      catch {
        Write-Warn "Scheduled-task launch failed: $($_.Exception.Message)"
      }
      finally {
        Start-Sleep -Seconds 2
        Unregister-ScheduledTask -TaskName $bootstrap -Confirm:$false -ErrorAction SilentlyContinue
      }
    }

    $guiProc = $null
    for ($i = 0; $i -lt 20; $i++) {
      $guiProc = @(Get-DeskflowGuiProcess)
      if ($guiProc.Count -gt 0) { break }
      Start-Sleep -Milliseconds 250
    }

    if ($guiProc -and $guiProc.Count -gt 0) {
      $p = $guiProc[0]
      Write-Host ("Tray GUI running: deskflow.exe pid={0} session={1}" -f $p.Id, $p.SessionId) -ForegroundColor Green
    }
    else {
      Write-Warn "Tray GUI did NOT stay running. Install WebView2, then run: `"$GuiPath`""
    }

    if ($RegisterLogon) {
      $logonTask = 'DeskflowTray'
      Unregister-ScheduledTask -TaskName $logonTask -Confirm:$false -ErrorAction SilentlyContinue
      try {
        $action = New-ScheduledTaskAction -Execute $GuiPath -WorkingDirectory $workDir
        $principal = New-ScheduledTaskPrincipal -UserId $UserName -LogonType Interactive -RunLevel Limited
        $trigger = New-ScheduledTaskTrigger -AtLogOn -User $UserName
        $settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit ([TimeSpan]::Zero)
        Register-ScheduledTask -TaskName $logonTask -Action $action -Principal $principal -Trigger $trigger -Settings $settings -Force | Out-Null
        Write-Host "Registered logon autostart task '$logonTask' for $UserName" -ForegroundColor Green
      }
      catch {
        Write-Warn "Could not register logon autostart: $($_.Exception.Message)"
      }
    }
  }
  finally {
    $ErrorActionPreference = $prevEap
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
    guiMode      = $(if ($script:UseTauriUi) { 'tauri' } else { 'qt' })
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

function Copy-Licenses([string] $StagingDir) {
  foreach ($licenseName in @('LICENSE', 'LICENSE_EXCEPTION')) {
    $repoLicense = Join-Path $script:RepoRoot $licenseName
    if (Test-Path $repoLicense) {
      Copy-Item $repoLicense (Join-Path $StagingDir $licenseName) -Force
    }
    elseif (Test-Path (Join-Path $InstallPath $licenseName)) {
      Copy-Item (Join-Path $InstallPath $licenseName) (Join-Path $StagingDir $licenseName) -Force
    }
  }
}

function Prepare-StagingQt([string] $StagingDir, [string] $QtBin) {
  Write-Step "Preparing staged Qt release in $StagingDir"

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

  Copy-Licenses $StagingDir

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

  if ($IncludeTauriUi -and -not $script:UseTauriUi) {
    $tauriUi = Join-Path $script:SourceDir 'deskflow-ui.exe'
    if (-not (Test-Path $tauriUi)) {
      Write-Warn "IncludeTauriUi set but deskflow-ui.exe not found at $tauriUi"
    }
    else {
      Copy-Item $tauriUi (Join-Path $StagingDir 'deskflow-ui.exe') -Force
      Write-Host "Staged side-by-side Tauri UI: deskflow-ui.exe" -ForegroundColor Green
    }
  }
}

function Prepare-StagingTauri([string] $StagingDir) {
  Write-Step "Preparing staged Tauri release in $StagingDir"

  if (Test-Path $StagingDir) {
    Remove-Item $StagingDir -Recurse -Force
  }
  New-Item -ItemType Directory -Path $StagingDir -Force | Out-Null

  $tauriSrc = Join-Path $script:SourceDir 'deskflow-ui.exe'
  if (-not (Test-Path $tauriSrc)) {
    throw "Missing Tauri UI: $tauriSrc (run .\migrate-windows.ps1 -BuildTauriUi or ui\scripts\build-windows.ps1)."
  }

  $requiredCore = @('deskflow-core.exe', 'deskflow-daemon.exe')
  foreach ($file in $requiredCore) {
    $src = Join-Path $script:SourceDir $file
    if (-not (Test-Path $src)) {
      throw "Missing build artifact: $src (run .\build-windows.ps1 first)."
    }
    Copy-Item $src (Join-Path $StagingDir $file)
  }

  # Install as deskflow.exe so Start Menu / existing shortcuts / habits keep working.
  Copy-Item $tauriSrc (Join-Path $StagingDir 'deskflow.exe') -Force
  # Also keep the explicit name for clarity / scripts.
  Copy-Item $tauriSrc (Join-Path $StagingDir 'deskflow-ui.exe') -Force

  # Tiny marker so we can tell installs apart later.
  $marker = [ordered]@{
    gui        = 'tauri'
    versionId  = $script:NewVersion
    migratedAt = (Get-Date).ToString('o')
  }
  $marker | ConvertTo-Json | Set-Content (Join-Path $StagingDir 'deskflow-ui.json') -Encoding UTF8

  Copy-Licenses $StagingDir

  # deskflow-core still links Qt (Widgets/Network/Core). Deploy Qt runtime for core only;
  # the Tauri GUI does not need the Qt GUI binary, but /MIR would delete these if omitted.
  $qtBin = Find-QtBin $script:RepoRoot
  $windeployqt = Join-Path $qtBin 'windeployqt.exe'
  $coreExe = Join-Path $StagingDir 'deskflow-core.exe'
  if ($PSCmdlet.ShouldProcess($coreExe, 'Run windeployqt for deskflow-core')) {
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
        $coreExe 2>&1 | ForEach-Object {
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
      throw "windeployqt failed for deskflow-core.exe (exit $LASTEXITCODE)."
    }
  }

  Remove-Item (Join-Path $StagingDir 'plugins\plugins') -Recurse -Force -ErrorAction SilentlyContinue

  # OpenSSL is a hard dependency of deskflow-core (not always pulled by windeployqt).
  foreach ($dll in @('libssl-3-x64.dll', 'libcrypto-3-x64.dll')) {
    $fromBuild = Join-Path $script:SourceDir $dll
    $fromQt = Join-Path $qtBin $dll
    $dest = Join-Path $StagingDir $dll
    if (Test-Path $fromBuild) {
      Copy-Item $fromBuild $dest -Force
    }
    elseif (Test-Path $fromQt) {
      Copy-Item $fromQt $dest -Force
    }
  }

  Write-Host "Staged Tauri GUI as deskflow.exe (+ deskflow-core/daemon + Qt runtime for core)." -ForegroundColor Green
}

function Install-StagedRelease([string] $StagingDir) {
  Write-Step "Installing into $InstallPath"

  if (-not (Test-Path $InstallPath)) {
    New-Item -ItemType Directory -Path $InstallPath -Force | Out-Null
  }

  if ($PSCmdlet.ShouldProcess($InstallPath, 'Deploy staged Deskflow release')) {
    # /MIR mirrors staging into install — removes files not in staging.
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

if ($BuildTauriUi) {
  $UseTauriUi = $true
}
$script:UseTauriUi = [bool]$UseTauriUi

if ($IncludeTauriUi -and $script:UseTauriUi) {
  Write-Warn "IncludeTauriUi is ignored when UseTauriUi is set (cutover replaces Qt GUI)."
}

if (-not $SourceDir) {
  $SourceDir = Join-Path $script:RepoRoot 'build\bin'
}

# Allow SourceDir to be created by a subsequent build step.
if (-not (Test-Path $SourceDir)) {
  if ($BuildFirst -or $BuildTauriUi) {
    New-Item -ItemType Directory -Path $SourceDir -Force | Out-Null
  }
  else {
    throw "Source directory not found: $SourceDir (run with -BuildFirst and/or -BuildTauriUi)."
  }
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
Write-Host "  GUI mode:       $(if ($script:UseTauriUi) { 'Tauri (cutover)' } elseif ($IncludeTauriUi) { 'Qt + side-by-side Tauri' } else { 'Qt' })"
Write-Host "  BuildFirst:     $BuildFirst"
Write-Host "  BuildTauriUi:   $BuildTauriUi"
Write-Host "  Force:          $Force"
Write-Host "  Settings:       $(if ($settingsFile) { $settingsFile } else { '(not found)' })"
Write-Host "  Install path:   $InstallPath"
Write-Host "  Source build:   $script:SourceDir"
Write-Host "  Remote server:  $(if ($remoteHost) { $remoteHost } else { '(none configured)' })"
Write-Host "  Advanced layout: $(if ($hasAdvanced) { 'enabled in config' } else { 'not configured (legacy links)' })"

$oldExe = Join-Path $InstallPath 'deskflow.exe'
$script:OldVersion = Get-DeskflowVersion $oldExe
if ($script:UseTauriUi) {
  $script:NewVersion = Get-TauriVersionId $script:RepoRoot
} else {
  $script:NewVersion = Get-DeskflowVersion (Join-Path $script:SourceDir 'deskflow.exe')
}
Write-Host "  Installed ver:  $(if ($script:OldVersion) { $script:OldVersion } else { '(no prior install)' })"
Write-Host "  Built ver:      $(if ($script:NewVersion) { $script:NewVersion } else { '(will build / unknown)' })"

if ($script:Role -eq 'Client' -and -not $Force -and -not $WhatIfPreference) {
  Write-Warn @"
This machine is configured as a CLIENT. This build uses protocol 1.9.
Upgrade the SERVER ($remoteHost) to this build BEFORE migrating clients,
or the server may reject the connection.

Re-run with -Force (default) or omit -Force:$false to migrate this machine.
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
  Write-Host "  4. .\migrate-windows.ps1"
  if ($script:UseTauriUi) {
    Write-Host "  Default is Tauri cutover (build C++/UI, replace deskflow.exe, remove Qt GUI runtime)."
  } else {
    Write-Host "  Qt GUI migrate selected (-UseTauriUi:$false)."
  }
  return
}

if ($BuildFirst) {
  Write-Step "Building latest C++ release"
  $buildScript = Join-Path $script:RepoRoot 'build-windows.ps1'
  if (-not (Test-Path $buildScript)) {
    throw "Missing build script: $buildScript"
  }
  & $buildScript -SkipQtInstall -SkipVcpkgBootstrap
  if ($LASTEXITCODE -ne 0) {
    throw "build-windows.ps1 failed."
  }
}

if ($BuildTauriUi) {
  Write-Step "Building Tauri UI"
  $uiBuild = Join-Path $script:RepoRoot 'ui\scripts\build-windows.ps1'
  if (-not (Test-Path $uiBuild)) {
    throw "Missing Tauri build script: $uiBuild"
  }
  $prevEap = $ErrorActionPreference
  $ErrorActionPreference = 'Continue'
  & $uiBuild
  $uiExit = $LASTEXITCODE
  $ErrorActionPreference = $prevEap
  if ($uiExit -ne 0) {
    throw "ui/scripts/build-windows.ps1 failed."
  }
}

if ($script:UseTauriUi) {
  $tauriExe = Join-Path $script:SourceDir 'deskflow-ui.exe'
  if (-not (Test-Path $tauriExe)) {
    throw "Built deskflow-ui.exe was not found at $tauriExe. Re-run with -BuildTauriUi (default)."
  }
  $script:NewVersion = Get-TauriVersionId $script:RepoRoot
  if (-not (Test-Path (Join-Path $script:SourceDir 'deskflow-core.exe'))) {
    throw "deskflow-core.exe missing in $script:SourceDir (run with -BuildFirst, the default)."
  }
  if (-not (Test-Path (Join-Path $script:SourceDir 'deskflow-daemon.exe'))) {
    throw "deskflow-daemon.exe missing in $script:SourceDir (run with -BuildFirst, the default)."
  }
  if (-not (Test-WebView2Installed)) {
    Write-Warn "WebView2 Runtime was not detected. The Tauri GUI needs it: https://developer.microsoft.com/microsoft-edge/webview2/"
    if (-not $Force) {
      throw "WebView2 Runtime required for Tauri UI. Install it, or re-run with -Force to proceed anyway."
    }
  }
}
else {
  $newExe = Join-Path $script:SourceDir 'deskflow.exe'
  $script:NewVersion = Get-DeskflowVersion $newExe
  if (-not $script:NewVersion) {
    throw "Built deskflow.exe was not found or could not report a version."
  }
}

Write-Host "  Using build ver: $(if ($script:NewVersion) { $script:NewVersion } else { '(unknown)' })"

$wasRunning = $false
$service = Get-Service -Name 'Deskflow' -ErrorAction SilentlyContinue
if ($service -and $service.Status -eq 'Running') {
  $wasRunning = $true
}

$stagingDir = Join-Path $script:RepoRoot ".migration-staging-$timestamp"

try {
  if (-not $SkipBackup) {
    New-Backup $timestamp
  }
  else {
    Write-Warn "Skipping backup (-SkipBackup)."
  }

  Stop-Deskflow

  if ($script:UseTauriUi) {
    Prepare-StagingTauri $stagingDir
  }
  else {
    $qtBin = Find-QtBin $script:RepoRoot
    Prepare-StagingQt $stagingDir $qtBin
  }

  Install-StagedRelease $stagingDir

  $installedGui = Join-Path $InstallPath 'deskflow.exe'
  $installedVersion = if ($script:UseTauriUi) {
    Get-TauriVersionId $script:RepoRoot
  } else {
    Get-DeskflowVersion $installedGui
  }

  $markerPath = Join-Path $InstallPath 'deskflow-ui.json'
  Write-Host ""
  Write-Host "Migration complete." -ForegroundColor Green
  Write-Host "  Version:  $installedVersion"
  Write-Host "  GUI:      $(if ($script:UseTauriUi) { 'Tauri (deskflow.exe)' } else { 'Qt (deskflow.exe)' })"
  Write-Host "  Backup:   $script:BackupDir"
  Write-Host "  Install:  $InstallPath"
  if ($script:UseTauriUi -and (Test-Path $markerPath)) {
    Write-Host "  Marker:   $markerPath"
  }

  if ($script:Role -eq 'Server') {
    Write-Host ""
    Write-Host "Next steps (server):"
    Write-Host "  1. Open Deskflow (Start Menu / $InstallPath\deskflow.exe)."
    Write-Host "  2. Existing screen links still work unchanged."
    Write-Host "  3. Optional: Edit layout -> Advanced display layout for per-monitor routing."
    Write-Host "  4. Upgrade each client machine with .\migrate-windows.ps1"
  }
  elseif ($script:Role -eq 'Client') {
    Write-Host ""
    Write-Host "Next steps (client):"
    Write-Host "  1. Ensure the server at $remoteHost is already on this build."
    Write-Host "  2. Confirm connection from the Deskflow GUI or tray icon."
  }

  if (-not $hasAdvanced) {
    Write-Host ""
    Write-Host "Your existing links-based layout is unchanged. Advanced monitor layout is opt-in."
  }

  $gui = Join-Path $InstallPath 'deskflow.exe'
  $user = Get-InteractiveUserName

  if ($script:UseTauriUi -and -not $NoStart) {
    # Tauri default is Desktop mode: GUI owns deskflow-core. Restarting the
    # Windows service leaves a Session-0 daemon/core the tray is not attached to.
    $svc = Get-Service -Name 'Deskflow' -ErrorAction SilentlyContinue
    if ($svc -and $svc.Status -eq 'Running') {
      if ($PSCmdlet.ShouldProcess('Deskflow service', 'Stop for Desktop-mode Tauri GUI')) {
        Stop-Service -Name 'Deskflow' -Force -ErrorAction SilentlyContinue
        sc.exe config Deskflow start= demand | Out-Null
      }
      Write-Host "  Service:  stopped (Desktop mode - GUI owns core)"
    }
    else {
      Write-Host "  Service:  left stopped (Desktop mode)"
    }

    if ($user) {
      Start-DeskflowGuiForUser -GuiPath $gui -UserName $user -RegisterLogon
    }
    else {
      Write-Host "No interactive user logged on; tray GUI not launched." -ForegroundColor Yellow
      Write-Host "After logon, run: `"$gui`"" -ForegroundColor Yellow
    }
  }
  elseif ($wasRunning -and -not $NoStart) {
    Start-DeskflowService
    Write-Host "  Service:  restarted (legacy Qt / service stack)"
    if ($user) {
      Start-DeskflowGuiForUser -GuiPath $gui -UserName $user -RegisterLogon
    }
  }
  elseif ($NoStart) {
    Write-Host "  Service:  left stopped (-NoStart)"
  }
}
finally {
  Remove-Item $stagingDir -Recurse -Force -ErrorAction SilentlyContinue
}
