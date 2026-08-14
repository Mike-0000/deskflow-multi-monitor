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

.PARAMETER ProcessMode
  Auto and Service both use the Automatic LocalSystem service for installed
  Windows builds. Desktop is an explicit portable/developer fallback and
  cannot inject input into elevated applications.

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
  [switch] $IncludeTauriUi,
  [ValidateSet('Auto', 'Service', 'Desktop')]
  [string] $ProcessMode = 'Auto'
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

function Invoke-VersionProbe([string] $ExePath, [int] $TimeoutMilliseconds = 5000) {
  if (-not (Test-Path $ExePath)) { return $null }

  $stdout = [System.IO.Path]::GetTempFileName()
  $stderr = [System.IO.Path]::GetTempFileName()
  $process = $null
  try {
    $process = Start-Process -FilePath $ExePath -ArgumentList '--version' -PassThru -NoNewWindow `
      -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if (-not $process.WaitForExit($TimeoutMilliseconds)) {
      $process.Kill()
      $process.WaitForExit()
      return $null
    }
    return ((Get-Content -LiteralPath $stdout -Raw -ErrorAction SilentlyContinue) +
      (Get-Content -LiteralPath $stderr -Raw -ErrorAction SilentlyContinue))
  } catch {
    return $null
  }
  finally {
    if ($process) { $process.Dispose() }
    Remove-Item -LiteralPath $stdout,$stderr -Force -ErrorAction SilentlyContinue
  }
}

function Get-DeskflowVersion([string] $ExePath) {
  if (-not (Test-Path $ExePath)) { return $null }

  # Prefer the PE version resource. In particular, a Tauri GUI is a Windows GUI
  # process: invoking it with --version opens the application and never returns.
  try {
    $vi = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($ExePath)
    if ($vi.ProductVersion) { return $vi.ProductVersion }
    if ($vi.FileVersion) { return $vi.FileVersion }
  } catch {}

  $output = Invoke-VersionProbe $ExePath
  if ($output -match 'Deskflow:\s+(\S+)') {
    return $Matches[1]
  }

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

function Get-VersionNumberPart([string] $VersionId) {
  if (-not $VersionId) { return $null }
  # "1.26.0.215+ba35f21f" or "v1.26.0.215" -> "1.26.0.215"
  $v = $VersionId.Trim()
  if ($v.StartsWith('v')) { $v = $v.Substring(1) }
  $plus = $v.IndexOf('+')
  if ($plus -ge 0) { $v = $v.Substring(0, $plus) }
  $space = $v.IndexOf(' ')
  if ($space -ge 0) { $v = $v.Substring(0, $space) }
  return $v
}

function Get-CoreVersionIdFromExe([string] $ExePath) {
  if (-not (Test-Path $ExePath)) { return $null }
  $output = Invoke-VersionProbe $ExePath
  if ($output) {
    # deskflow-core.exe v1.26.0.214 (4b4f3dd9), protocol v1.9
    if ($output -match 'v(\d+\.\d+\.\d+(?:\.\d+)?)\s+\(([0-9a-fA-F]+)\)') {
      return "$($Matches[1])+$($Matches[2])"
    }
    if ($output -match '(\d+\.\d+\.\d+(?:\.\d+)?)') {
      return $Matches[1]
    }
  }
  return Get-DeskflowVersion $ExePath
}

function Get-DaemonVersionIdFromExe([string] $ExePath) {
  if (-not (Test-Path $ExePath)) { return $null }
  $output = Invoke-VersionProbe $ExePath
  if ($output) {
    if ($output -match '(\d+\.\d+\.\d+(?:\.\d+)?)\+([0-9a-fA-F]+)') {
      return "$($Matches[1])+$($Matches[2])"
    }
  }
  return Get-DeskflowVersion $ExePath
}

function Assert-BinaryVersionAlignment {
  param(
    [Parameter(Mandatory)][string] $GuiVersionId,
    [Parameter(Mandatory)][string] $CoreExe,
    [Parameter(Mandatory)][string] $DaemonExe,
    [string] $Context = 'install'
  )

  $guiNum = Get-VersionNumberPart $GuiVersionId
  if (-not $guiNum) {
    throw "Cannot parse GUI versionId '$GuiVersionId' for $Context alignment check."
  }

  $coreId = Get-CoreVersionIdFromExe $CoreExe
  $daemonId = Get-DaemonVersionIdFromExe $DaemonExe
  $coreNum = Get-VersionNumberPart $coreId
  $daemonNum = Get-VersionNumberPart $daemonId

  Write-Host "  Version alignment ($Context):" -ForegroundColor Cyan
  Write-Host "    GUI:    $GuiVersionId"
  Write-Host "    core:   $(if ($coreId) { $coreId } else { '(unknown)' })"
  Write-Host "    daemon: $(if ($daemonId) { $daemonId } else { '(unknown)' })"

  $mismatches = @()
  if (-not $coreNum) {
    $mismatches += "could not read version from $CoreExe"
  } elseif ($coreId -ne $GuiVersionId) {
    $mismatches += "core $coreId != GUI $GuiVersionId"
  }
  if (-not $daemonNum) {
    $mismatches += "could not read version from $DaemonExe"
  } elseif ($daemonId -ne $GuiVersionId) {
    $mismatches += "daemon $daemonId != GUI $GuiVersionId"
  }

  if ($mismatches.Count -gt 0) {
    throw @"
Version skew detected during $Context — this causes IPC mismatch warnings, skipped hello/state replay, and slow/flaky Start.

  $($mismatches -join "`n  ")

Rebuild C++ and Tauri together (do not use UI-only migrate), then re-run:
  .\migrate-windows.ps1 -Force
  # or: .\build-windows.ps1 ; then migrate with -BuildFirst (default)
"@
  }

  Write-Host "  GUI/core/daemon exact versions match ($GuiVersionId)." -ForegroundColor Green
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

function Get-InteractiveUserProfilePath {
  param([Parameter(Mandatory = $true)][string] $UserName)
  try {
    $sid = (New-Object System.Security.Principal.NTAccount($UserName)).Translate(
      [System.Security.Principal.SecurityIdentifier]
    ).Value
    $profile = Get-CimInstance Win32_UserProfile -ErrorAction SilentlyContinue |
      Where-Object { $_.SID -eq $sid -and $_.LocalPath } |
      Select-Object -First 1
    if ($profile) { return $profile.LocalPath }
  }
  catch {}
  return $null
}

function Set-DeskflowIniValue {
  param(
    [Parameter(Mandatory = $true)][string] $Path,
    [Parameter(Mandatory = $true)][string] $Section,
    [Parameter(Mandatory = $true)][string] $Key,
    [Parameter(Mandatory = $true)][string] $Value
  )

  $dir = Split-Path -Parent $Path
  if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
  $lines = if (Test-Path $Path) { @(Get-Content -LiteralPath $Path) } else { @() }
  $currentSection = ''
  $found = $false
  $out = New-Object System.Collections.Generic.List[string]
  foreach ($line in $lines) {
    if ($line -match '^\s*\[([^\]]+)\]\s*$') {
      $currentSection = $Matches[1]
      $out.Add($line)
      continue
    }
    if ($line -match ("^\s*" + [regex]::Escape("$Section/$Key") + "\s*=")) {
      # Remove the malformed Tauri V2 representation. QSettings expects a
      # section and leaf key, not a slash-path entry in [General].
      continue
    }
    if ($currentSection -eq $Section -and $line -match ("^\s*" + [regex]::Escape($Key) + "\s*=")) {
      $out.Add("$Key=$Value")
      $found = $true
      continue
    }
    $out.Add($line)
  }
  if (-not $found) {
    $out.Add("[$Section]")
    $out.Add("$Key=$Value")
  }
  Set-Content -LiteralPath $Path -Value $out.ToArray() -Encoding UTF8
}

function Resolve-CanonicalSettingsFile {
  param([string] $UserName, [string] $ExistingSettingsFile)
  if ($UserName) {
    $profile = Get-InteractiveUserProfilePath -UserName $UserName
    if ($profile) {
      $canonical = Join-Path $profile 'AppData\Roaming\Deskflow\Deskflow.conf'
      if (-not (Test-Path $canonical) -and $ExistingSettingsFile -and (Test-Path $ExistingSettingsFile)) {
        $dir = Split-Path -Parent $canonical
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
        Copy-Item -LiteralPath $ExistingSettingsFile -Destination $canonical -Force
      }
      return $canonical
    }
  }
  if ($ExistingSettingsFile) { return $ExistingSettingsFile }
  return (Join-Path $env:ProgramData 'Deskflow\Deskflow.conf')
}

function Invoke-DeskflowDaemonStart {
  param(
    [Parameter(Mandatory = $true)][string] $VersionId,
    [Parameter(Mandatory = $true)][string] $SettingsFile
  )
  $pipe = New-Object System.IO.Pipes.NamedPipeClientStream(
    '.', 'deskflow-daemon',
    [System.IO.Pipes.PipeDirection]::InOut,
    [System.IO.Pipes.PipeOptions]::Asynchronous
  )
  $reader = $null
  $writer = $null
  $lastStatus = 'no status received'
  try {
    $pipe.Connect(10000)
    $utf8 = [System.Text.UTF8Encoding]::new($false)
    $reader = New-Object System.IO.StreamReader($pipe, $utf8, $false, 4096, $true)
    $writer = New-Object System.IO.StreamWriter($pipe, $utf8, 4096, $true)
    $writer.NewLine = "`n"
    $writer.AutoFlush = $true
    $writer.WriteLine("hello=$VersionId")
    $hello = $reader.ReadLine()
    if ($hello -ne "hello=$VersionId") { throw "Daemon handshake failed: $hello" }

    foreach ($request in @(
      @{ Message = "configFile=$SettingsFile"; Ack = 'ok=configFile' },
      @{ Message = 'start'; Ack = 'ok=start' }
    )) {
      $writer.WriteLine($request.Message)
      $response = $reader.ReadLine()
      if ($response -ne $request.Ack) { throw "Daemon rejected '$($request.Message)': $response" }
    }

    $deadline = [DateTime]::UtcNow.AddSeconds(25)
    do {
      $writer.WriteLine('status')
      $response = $reader.ReadLine()
      if ($response -like 'status=*') {
        $status = $response.Substring(7) | ConvertFrom-Json
        if ($status.state -eq 'Running' -and [int64]$status.processId -gt 0) {
          Write-Host "  Core:     service-owned PID $($status.processId), session $($status.sessionId), integrity $($status.integrityRid)"
          return
        }
        $lastStatus = if ($status.lastError) { "$($status.state): $($status.lastError)" } else { [string]$status.state }
      }
      Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Service core did not become ready: $lastStatus"
  }
  finally {
    if ($writer) { $writer.Dispose() }
    if ($reader) { $reader.Dispose() }
    $pipe.Dispose()
  }
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
    # This script runs elevated. A direct Start-Process would inherit that token,
    # defeating the intended split between a limited GUI and service-owned core.
    $bootstrap = 'DeskflowGuiBootstrap'
    Unregister-ScheduledTask -TaskName $bootstrap -Confirm:$false -ErrorAction SilentlyContinue
    $action = New-ScheduledTaskAction -Execute $GuiPath -WorkingDirectory $workDir
    $principal = New-ScheduledTaskPrincipal -UserId $UserName -LogonType Interactive -RunLevel Limited
    $trigger = New-ScheduledTaskTrigger -Once -At (Get-Date).AddMinutes(-1)
    $settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -StartWhenAvailable
    try {
      Register-ScheduledTask -TaskName $bootstrap -Action $action -Principal $principal -Trigger $trigger -Settings $settings -Force | Out-Null
      Start-ScheduledTask -TaskName $bootstrap -ErrorAction SilentlyContinue
      Write-Host "Started limited deskflow.exe via scheduled task for $UserName." -ForegroundColor Cyan
    }
    catch {
      Write-Warn "Limited scheduled-task launch failed: $($_.Exception.Message)"
    }
    finally {
      Start-Sleep -Seconds 2
      Unregister-ScheduledTask -TaskName $bootstrap -Confirm:$false -ErrorAction SilentlyContinue
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

  $settingsBackup = Join-Path $script:BackupDir 'config'
  $configCandidates = [ordered]@{
    currentUser = Join-Path $env:APPDATA 'Deskflow\Deskflow.conf'
    system      = Join-Path $env:ProgramData 'Deskflow\Deskflow.conf'
    portable    = Join-Path $InstallPath 'settings\Deskflow.conf'
  }
  $interactiveUser = Get-InteractiveUserName
  if ($interactiveUser) {
    $interactiveProfile = Get-InteractiveUserProfilePath -UserName $interactiveUser
    if ($interactiveProfile) {
      $configCandidates.interactiveUser = Join-Path $interactiveProfile 'AppData\Roaming\Deskflow\Deskflow.conf'
    }
  }
  foreach ($entry in $configCandidates.GetEnumerator()) {
    if (Test-Path -LiteralPath $entry.Value) {
      New-Item -ItemType Directory -Path $settingsBackup -Force | Out-Null
      Copy-Item -LiteralPath $entry.Value -Destination (Join-Path $settingsBackup "$($entry.Key)-Deskflow.conf") -Force
    }
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

  # deskflow-cli: version-locked Core IPC helper used by Stream Deck (and scripts).
  $cliSrc = Join-Path $script:SourceDir 'deskflow-cli.exe'
  if (-not (Test-Path $cliSrc)) {
    $cliSrc = Join-Path $script:RepoRoot 'ui\src-tauri\target\release\deskflow-cli.exe'
  }
  if (-not (Test-Path $cliSrc)) {
    throw "Missing deskflow-cli.exe (expected under build\bin or ui\src-tauri\target\release). Re-run ui\scripts\build-windows.ps1."
  }
  Copy-Item $cliSrc (Join-Path $StagingDir 'deskflow-cli.exe') -Force

  # Official Stream Deck plugin (sideloaded by Install-Remote into the interactive user's Plugins folder).
  $sdPluginName = 'com.deskflow.control.sdPlugin'
  $sdPluginSrc = Join-Path $script:RepoRoot "streamdeck\$sdPluginName"
  $sdPluginJs = Join-Path $sdPluginSrc 'bin\plugin.js'
  if (-not (Test-Path $sdPluginJs)) {
    throw "Missing Stream Deck plugin at $sdPluginSrc (bin\plugin.js). Build streamdeck/ first (npm run pack)."
  }
  $sdDest = Join-Path $StagingDir "StreamDeck\$sdPluginName"
  New-Item -ItemType Directory -Path (Split-Path $sdDest -Parent) -Force | Out-Null
  if (Test-Path $sdDest) {
    Remove-Item $sdDest -Recurse -Force
  }
  Copy-Item $sdPluginSrc $sdDest -Recurse -Force

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

  # The core and daemon are linked against Qt, so verify executable output only
  # after their runtime dependencies have been deployed to the staging folder.
  $stagedCore = Join-Path $StagingDir 'deskflow-core.exe'
  $stagedDaemon = Join-Path $StagingDir 'deskflow-daemon.exe'
  Assert-BinaryVersionAlignment -GuiVersionId $script:NewVersion -CoreExe $stagedCore -DaemonExe $stagedDaemon -Context 'migrate staging'

  $coreVersionId = Get-CoreVersionIdFromExe $stagedCore
  $daemonVersionId = Get-DaemonVersionIdFromExe $stagedDaemon

  # Tiny marker so we can tell installs apart later.
  $marker = [ordered]@{
    gui           = 'tauri'
    versionId     = $script:NewVersion
    coreVersionId = $coreVersionId
    daemonVersion = $daemonVersionId
    migratedAt    = (Get-Date).ToString('o')
  }
  $marker | ConvertTo-Json | Set-Content (Join-Path $StagingDir 'deskflow-ui.json') -Encoding UTF8

  Write-Host "Staged Tauri GUI as deskflow.exe (+ deskflow-cli + Stream Deck plugin + deskflow-core/daemon + Qt runtime for core)." -ForegroundColor Green
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
Write-Host "  Process mode:   $ProcessMode $(if ($ProcessMode -eq 'Auto') { '(installed Windows defaults to Service)' })"
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
  # Catch UI-only rebuilds before staging: GUI version.json must match build\bin core/daemon.
  Assert-BinaryVersionAlignment `
    -GuiVersionId $script:NewVersion `
    -CoreExe (Join-Path $script:SourceDir 'deskflow-core.exe') `
    -DaemonExe (Join-Path $script:SourceDir 'deskflow-daemon.exe') `
    -Context 'build artifacts'
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

  $desiredMode = if ($ProcessMode -eq 'Desktop') { 'Desktop' } else { 'Service' }
  $canonicalSettings = Resolve-CanonicalSettingsFile -UserName $user -ExistingSettingsFile $settingsFile
  Set-DeskflowIniValue -Path $canonicalSettings -Section 'core' -Key 'processMode' -Value $(if ($desiredMode -eq 'Service') { '0' } else { '1' })
  Set-DeskflowIniValue -Path $canonicalSettings -Section 'gui' -Key 'startCoreWithGui' -Value 'true'
  if ($desiredMode -eq 'Service') {
    Set-DeskflowIniValue -Path $canonicalSettings -Section 'daemon' -Key 'elevate' -Value 'true'
  }

  $svc = Get-Service -Name 'Deskflow' -ErrorAction SilentlyContinue
  if ($desiredMode -eq 'Service') {
    if (-not $svc) {
      throw 'Deskflow service is missing after migration; repair the installation before launching the GUI.'
    }
    if ($PSCmdlet.ShouldProcess('Deskflow service', 'Configure automatic elevated core ownership')) {
      sc.exe config Deskflow start= auto obj= LocalSystem | Out-Null
      sc.exe description Deskflow "Runs the Core process on secure desktops (UAC prompts, login screen, etc)." | Out-Null
      sc.exe failure Deskflow reset= 86400 actions= restart/1000/restart/1000/none/0 | Out-Null
      sc.exe failureflag Deskflow 1 | Out-Null
    }
    if (-not $NoStart) {
      Start-DeskflowService
      Write-Host "  Service:  running (service owns elevated core)"
      if ((Read-DeskflowRole $canonicalSettings) -ne 'Unknown') {
        Invoke-DeskflowDaemonStart -VersionId $script:NewVersion -SettingsFile $canonicalSettings
      }
      else {
        Write-Host "  Core:     waiting for a client/server role to be saved in the GUI" -ForegroundColor Yellow
      }
    }
    else {
      Write-Host "  Service:  configured Automatic but left stopped (-NoStart)"
    }
  }
  else {
    if ($svc -and $svc.Status -eq 'Running') {
      Stop-Service -Name 'Deskflow' -Force -ErrorAction SilentlyContinue
    }
    sc.exe config Deskflow start= demand | Out-Null
    Write-Host "  Service:  stopped (explicit Desktop mode)"
  }

  if (-not $NoStart -and $user) {
    Start-DeskflowGuiForUser -GuiPath $gui -UserName $user -RegisterLogon
  }
  elseif (-not $NoStart) {
    Write-Host "No interactive user logged on; tray GUI not launched." -ForegroundColor Yellow
    Write-Host "The Automatic service will start the persisted core after it has received a configuration." -ForegroundColor Yellow
  }
}
finally {
  Remove-Item $stagingDir -Recurse -Force -ErrorAction SilentlyContinue
}
