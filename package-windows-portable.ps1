#Requires -Version 5.1
<#
.SYNOPSIS
  Package a portable Deskflow Windows build for machines without Visual Studio.

.DESCRIPTION
  Copies the current installed Deskflow tree (or a staging folder) into dist\,
  adds a remote install script + README, and creates a zip you can copy to
  another PC. The remote host needs only:
    - Visual C++ Redistributable 2015-2022 (x64)
    - WebView2 Runtime (for the Tauri GUI)
  No Visual Studio, CMake, Qt SDK, or vcpkg required.

.PARAMETER SourceDir
  Folder to package. Defaults to the installed Program Files\Deskflow tree.

.PARAMETER OutDir
  Where to write the zip (default: repo/dist).

.PARAMETER SkipZip
  Prepare the folder only; do not create the .zip.
#>
[CmdletBinding()]
param(
  [string] $SourceDir = "${env:ProgramFiles}\Deskflow",
  [string] $OutDir = '',
  [switch] $SkipZip
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = $PSScriptRoot
if (-not (Test-Path (Join-Path $repoRoot 'CMakeLists.txt'))) {
  throw "Run from the Deskflow repository root."
}

if (-not $OutDir) {
  $OutDir = Join-Path $repoRoot 'dist'
}

if (-not (Test-Path $SourceDir)) {
  throw "Source not found: $SourceDir. Run .\migrate-windows.ps1 first, or pass -SourceDir."
}

$required = @('deskflow.exe', 'deskflow-core.exe', 'deskflow-daemon.exe', 'Qt6Widgets.dll')
foreach ($name in $required) {
  if (-not (Test-Path (Join-Path $SourceDir $name))) {
    throw "Missing $name under $SourceDir. Re-run migrate so Qt runtime is deployed for deskflow-core."
  }
}
if (-not (Test-Path (Join-Path $SourceDir 'deskflow-cli.exe'))) {
  Write-Host "WARNING: deskflow-cli.exe missing under $SourceDir (Stream Deck Pause needs it). Re-run migrate." -ForegroundColor Yellow
}
$sdPluginCheck = Join-Path $SourceDir 'StreamDeck\com.deskflow.control.sdPlugin\manifest.json'
if (-not (Test-Path $sdPluginCheck)) {
  Write-Host "WARNING: Stream Deck plugin missing under $SourceDir\StreamDeck (Install-Remote will skip plugin install)." -ForegroundColor Yellow
}

function Get-VersionNumberPart([string] $VersionId) {
  if (-not $VersionId) { return $null }
  $v = $VersionId.Trim()
  if ($v.StartsWith('v')) { $v = $v.Substring(1) }
  $plus = $v.IndexOf('+')
  if ($plus -ge 0) { $v = $v.Substring(0, $plus) }
  $space = $v.IndexOf(' ')
  if ($space -ge 0) { $v = $v.Substring(0, $space) }
  return $v
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

function Get-CoreVersionIdFromExe([string] $ExePath) {
  if (-not (Test-Path $ExePath)) { return $null }
  $output = Invoke-VersionProbe $ExePath
  if ($output) {
    if ($output -match 'v(\d+\.\d+\.\d+(?:\.\d+)?)\s+\(([0-9a-fA-F]+)\)') {
      return "$($Matches[1])+$($Matches[2])"
    }
    if ($output -match '(\d+\.\d+\.\d+(?:\.\d+)?)') {
      return $Matches[1]
    }
  }
  try {
    $vi = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($ExePath)
    if ($vi.ProductVersion) { return $vi.ProductVersion }
  } catch {}
  return $null
}

function Get-DaemonVersionFromExe([string] $ExePath) {
  if (-not (Test-Path $ExePath)) { return $null }
  $output = Invoke-VersionProbe $ExePath
  if ($output) {
    if ($output -match '(\d+\.\d+\.\d+(?:\.\d+)?)\+([0-9a-fA-F]+)') {
      return "$($Matches[1])+$($Matches[2])"
    }
    if ($output -match '(\d+\.\d+\.\d+(?:\.\d+)?)') {
      return $Matches[1]
    }
  }
  try {
    $vi = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($ExePath)
    if ($vi.ProductVersion) { return $vi.ProductVersion }
  } catch {}
  return $null
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$versionHint = '1.26.0'
$marker = Join-Path $SourceDir 'deskflow-ui.json'
if (Test-Path $marker) {
  try {
    $json = Get-Content $marker -Raw | ConvertFrom-Json
    if ($json.versionId) { $versionHint = [string]$json.versionId }
  } catch {}
}

$guiNum = Get-VersionNumberPart $versionHint
$coreId = Get-CoreVersionIdFromExe (Join-Path $SourceDir 'deskflow-core.exe')
$daemonId = Get-DaemonVersionFromExe (Join-Path $SourceDir 'deskflow-daemon.exe')
$coreNum = Get-VersionNumberPart $coreId
$daemonNum = Get-VersionNumberPart $daemonId
Write-Host "Version check before package:" -ForegroundColor Cyan
Write-Host "  GUI:    $versionHint"
Write-Host "  core:   $(if ($coreId) { $coreId } else { '(unknown)' })"
Write-Host "  daemon: $(if ($daemonId) { $daemonId } else { '(unknown)' })"
if (-not $guiNum -or -not $coreNum -or -not $daemonNum -or $coreId -ne $versionHint -or $daemonId -ne $versionHint) {
  throw @"
Refusing to package version-skewed Deskflow tree under $SourceDir.
GUI/core/daemon must share the exact version and Git SHA (got GUI=$versionHint core=$coreId daemon=$daemonId).
Re-run .\migrate-windows.ps1 with a full C++ + Tauri build first.
"@
}
Write-Host "  Aligned at $guiNum - packaging." -ForegroundColor Green

$pkgName = "deskflow-$versionHint-win-x64-portable"
$stageRoot = Join-Path $OutDir $pkgName
$payload = Join-Path $stageRoot 'Deskflow'

Write-Host "==> Staging portable package" -ForegroundColor Cyan
Write-Host "    Source: $SourceDir"
Write-Host "    Stage:  $stageRoot"

if (Test-Path $stageRoot) {
  Remove-Item $stageRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $payload -Force | Out-Null

robocopy $SourceDir $payload /E /R:2 /W:2 /NFL /NDL /NJH /NJS /NC /NS | Out-Null
if ($LASTEXITCODE -ge 8) {
  throw "robocopy failed (exit $LASTEXITCODE)"
}

$qtConf = Join-Path $payload 'qt.conf'
if (-not (Test-Path $qtConf)) {
  @"
[Paths]
Prefix = .
Plugins = plugins
"@ | Set-Content -Path $qtConf -Encoding ASCII
}

$installScript = @'
#Requires -Version 5.1
#Requires -RunAsAdministrator
<#
.SYNOPSIS
  Install a portable Deskflow package on this machine (no Visual Studio needed).

.DESCRIPTION
  Mode selection (first match wins):
    -Desktop explicitly selects tray-owned Desktop mode
    -NoService selects Desktop mode for portable/developer use
    All normal installed Windows clients and servers -> Service

  Service mode starts deskflow-daemon so the elevated core can interact with
  high-integrity and secure-desktop windows. Desktop mode is an explicit
  portable/developer fallback and cannot control elevated applications.

.PARAMETER InstallPath
  Target install directory.

.PARAMETER UseService
  Force the Windows daemon stack (processMode=Service).

.PARAMETER Desktop
  Force Desktop mode (tray owns core). Not recommended for always-on servers.

.PARAMETER NoStart
  With Service mode, register/configure but do not start the service.

.PARAMETER NoService
  Only copy files; do not create/update the Windows service.

.PARAMETER NoGui
  Do not launch or register the tray GUI.
#>
[CmdletBinding()]
param(
  [string] $InstallPath = "${env:ProgramFiles}\Deskflow",
  [switch] $UseService,
  [switch] $Desktop,
  [switch] $NoStart,
  [switch] $NoService,
  [switch] $NoGui
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ($UseService -and ($Desktop -or $NoService)) {
  throw "-UseService cannot be combined with -Desktop or -NoService."
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
  $leaf = ($UserName -split '\\')[-1]
  $guess = Join-Path $env:SystemDrive "Users\$leaf"
  if (Test-Path $guess) { return $guess }
  return $null
}

function Get-DeskflowConfPaths {
  param([string] $UserName)
  $paths = New-Object System.Collections.Generic.List[string]
  $paths.Add((Join-Path $env:ProgramData 'Deskflow\Deskflow.conf'))
  if ($UserName) {
    $profile = Get-InteractiveUserProfilePath -UserName $UserName
    if ($profile) {
      $paths.Add((Join-Path $profile 'AppData\Roaming\Deskflow\Deskflow.conf'))
    }
  }
  return $paths
}

function Get-DeskflowIniValue {
  param(
    [Parameter(Mandatory = $true)][string] $Path,
    [Parameter(Mandatory = $true)][string] $Section,
    [Parameter(Mandatory = $true)][string] $Key
  )
  if (-not (Test-Path $Path)) { return $null }
  $section = ''
  foreach ($line in @(Get-Content -Path $Path -ErrorAction SilentlyContinue)) {
    if ($line -match '^\s*\[([^\]]+)\]\s*$') {
      $section = $matches[1]
      continue
    }
    if ($section -eq $Section -and $line -match ("^\s*" + [regex]::Escape($Key) + "\s*=\s*(.*)$")) {
      return $matches[1].Trim()
    }
    if ($line -match ("^\s*" + [regex]::Escape("$Section/$Key") + "\s*=\s*(.*)$")) {
      return $matches[1].Trim()
    }
  }
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
  if (-not (Test-Path $dir)) {
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
  }
  if (-not (Test-Path $Path)) {
    Set-Content -Path $Path -Value @("[$Section]", "$Key=$Value") -Encoding ASCII
    return
  }

  $section = ''
  $found = $false
  $out = New-Object System.Collections.Generic.List[string]
  foreach ($line in @(Get-Content -Path $Path -ErrorAction SilentlyContinue)) {
    if ($line -match '^\s*\[([^\]]+)\]\s*$') {
      $section = $matches[1]
      $out.Add($line)
      continue
    }
    if ($section -eq $Section -and $line -match ("^\s*" + [regex]::Escape($Key) + "\s*=")) {
      $out.Add("$Key=$Value")
      $found = $true
      continue
    }
    if ($line -match ("^\s*" + [regex]::Escape("$Section/$Key") + "\s*=")) {
      # Tauri V2 wrote slash-path keys into [General]. QSettings cannot read
      # those entries, so remove them and write the canonical section below.
      continue
    }
    $out.Add($line)
  }
  if (-not $found) {
    $out.Add("[$Section]")
    $out.Add("$Key=$Value")
  }
  Set-Content -Path $Path -Value $out.ToArray() -Encoding ASCII
}

function Set-DeskflowProcessMode {
  param(
    [Parameter(Mandatory = $true)][ValidateSet('Desktop', 'Service')][string] $Mode,
    [string] $UserName
  )
  $val = if ($Mode -eq 'Service') { '0' } else { '1' }
  foreach ($path in (Get-DeskflowConfPaths -UserName $UserName)) {
    Set-DeskflowIniValue -Path $path -Section 'core' -Key 'processMode' -Value $val
    if ($Mode -eq 'Desktop') {
      # Desktop mode is useless unless the GUI actually starts core.
      Set-DeskflowIniValue -Path $path -Section 'gui' -Key 'startCoreWithGui' -Value 'true'
    }
    Write-Host "Set $Mode process mode in $path" -ForegroundColor Cyan
  }
}

function Resolve-DeskflowInstallMode {
  param(
    [string] $UserName,
    [switch] $ForceService,
    [switch] $ForceDesktop
  )
  if ($ForceService) { return 'Service' }
  if ($ForceDesktop) { return 'Desktop' }

  Write-Host "Using Service mode for installed Windows (elevated input and secure desktops)." -ForegroundColor Cyan
  return 'Service'
}

function Test-DeskflowCoreAlive {
  if (Get-Process -Name 'deskflow-core' -ErrorAction SilentlyContinue) { return $true }
  $listeners = Get-NetTCPConnection -LocalPort 24800 -State Listen -ErrorAction SilentlyContinue
  return [bool]$listeners
}

function Invoke-DeskflowDaemonStart {
  param(
    [Parameter(Mandatory = $true)][string] $VersionId,
    [Parameter(Mandatory = $true)][string] $SettingsFile,
    [string] $LogLevel = 'INFO'
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
    $reader = New-Object System.IO.StreamReader($pipe, [System.Text.UTF8Encoding]::new($false), $false, 4096, $true)
    $writer = New-Object System.IO.StreamWriter($pipe, [System.Text.UTF8Encoding]::new($false), 4096, $true)
    $writer.NewLine = "`n"
    $writer.AutoFlush = $true

    $writer.WriteLine("hello=$VersionId")
    $hello = $reader.ReadLine()
    if ($hello -ne "hello=$VersionId") { throw "Daemon handshake failed: $hello" }

    foreach ($request in @(
      @{ Message = "logLevel=$LogLevel"; Ack = 'ok=logLevel' },
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
          Write-Host "Service core ready (PID=$($status.processId), session=$($status.sessionId), integrity=$($status.integrityRid))." -ForegroundColor Green
          return
        }
        if ($status.lastError) { $lastStatus = "$($status.state): $($status.lastError)" }
        else { $lastStatus = [string]$status.state }
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

function Ensure-DeskflowFirewallRules {
  param(
    [Parameter(Mandatory = $true)][string] $CorePath,
    [int] $Port = 24800
  )

  # Port-based rule: listening alone is not enough; Windows Firewall often drops
  # inbound 24800 while Get-NetTCPConnection still shows Listen (clients SynSent/timeout).
  $portRule = 'Deskflow TCP 24800 Inbound'
  $existingPort = Get-NetFirewallRule -DisplayName $portRule -ErrorAction SilentlyContinue
  if (-not $existingPort) {
    New-NetFirewallRule -DisplayName $portRule `
      -Direction Inbound -Action Allow -Protocol TCP -LocalPort $Port `
      -Profile Any `
      -Description 'Allow remote Deskflow clients to connect to the server on TCP 24800' | Out-Null
    Write-Host "Created firewall rule: $portRule (TCP $Port inbound)" -ForegroundColor Green
  }
  else {
    Set-NetFirewallRule -DisplayName $portRule -Enabled True -Action Allow -ErrorAction SilentlyContinue
    Write-Host "Firewall rule already present (enabled): $portRule" -ForegroundColor Cyan
  }

  # Program-based rules match the classic Deskflow installer pattern.
  if (Test-Path $CorePath) {
    foreach ($name in @('Deskflow Server', 'Deskflow Client')) {
      $existing = Get-NetFirewallRule -DisplayName $name -ErrorAction SilentlyContinue
      if (-not $existing) {
        New-NetFirewallRule -DisplayName $name `
          -Direction Inbound -Action Allow -Program $CorePath `
          -Profile Any `
          -Description 'Allow Deskflow core network access' | Out-Null
        Write-Host "Created firewall rule: $name -> $CorePath" -ForegroundColor Green
      }
      else {
        Set-NetFirewallRule -DisplayName $name -Enabled True -Action Allow -ErrorAction SilentlyContinue
        try {
          Set-NetFirewallApplicationFilter -DisplayName $name -Program $CorePath -ErrorAction SilentlyContinue
        } catch {}
      }
    }
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
    Write-Host "GUI binary missing: $GuiPath" -ForegroundColor Yellow
    return
  }

  $workDir = Split-Path -Parent $GuiPath

  # Drop any stale GUI from a previous install (may be Session 0 / invisible).
  Get-Process -Name 'deskflow','deskflow-ui' -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -and ($_.Path -like '*\Deskflow\*') } |
    ForEach-Object {
      try { Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue } catch {}
    }
  Start-Sleep -Milliseconds 500

  $prevEap = $ErrorActionPreference
  $ErrorActionPreference = 'Continue'
  try {
    $launched = $false

    # Install-Remote runs elevated. Launching directly would give the tray GUI
    # the installer token, so bootstrap it explicitly at the user's limited level.
    $bootstrap = 'DeskflowGuiBootstrap'
    Unregister-ScheduledTask -TaskName $bootstrap -Confirm:$false -ErrorAction SilentlyContinue
    $action = New-ScheduledTaskAction -Execute $GuiPath -WorkingDirectory $workDir
    $principal = New-ScheduledTaskPrincipal -UserId $UserName -LogonType Interactive -RunLevel Limited
    $trigger = New-ScheduledTaskTrigger -Once -At (Get-Date).AddMinutes(-1)
    $settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -StartWhenAvailable
    try {
      Register-ScheduledTask -TaskName $bootstrap -Action $action -Principal $principal -Trigger $trigger -Settings $settings -Force | Out-Null
      Start-ScheduledTask -TaskName $bootstrap -ErrorAction SilentlyContinue
      $launched = $true
      Write-Host "Started limited deskflow.exe via scheduled task for $UserName." -ForegroundColor Cyan
    }
    catch {
      Write-Host "Limited scheduled-task launch failed: $($_.Exception.Message)" -ForegroundColor Yellow
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
      Write-Host "Look for the Deskflow icon in the notification area (overflow chevron if hidden)." -ForegroundColor Green
    }
    else {
      Write-Host "Tray GUI did NOT stay running (deskflow.exe missing from interactive session)." -ForegroundColor Red
      Write-Host "Install WebView2 Runtime, then run: `"$GuiPath`"" -ForegroundColor Yellow
      Write-Host "  https://developer.microsoft.com/microsoft-edge/webview2/" -ForegroundColor Yellow
      if (-not $launched) {
        Write-Host "No launch method succeeded for $UserName." -ForegroundColor Yellow
      }
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
        Write-Host "Could not register logon autostart: $($_.Exception.Message)" -ForegroundColor Yellow
      }
    }
  }
  finally {
    $ErrorActionPreference = $prevEap
  }
}

function Get-UserProfilePathForName {
  param([Parameter(Mandatory = $true)][string] $UserName)
  $short = if ($UserName -match '\\') { ($UserName -split '\\')[-1] } else { $UserName }
  $guess = Join-Path $env:SystemDrive "Users\$short"
  if (Test-Path $guess) { return $guess }
  $fromCim = Get-CimInstance Win32_UserProfile -ErrorAction SilentlyContinue |
    Where-Object { -not $_.Special -and $_.LocalPath -and ($_.LocalPath -like "*\$short") } |
    Select-Object -First 1
  if ($fromCim -and $fromCim.LocalPath) { return $fromCim.LocalPath }
  return $null
}

function Install-DeskflowStreamDeckPlugin {
  param(
    [Parameter(Mandatory = $true)][string] $InstallPath,
    [string] $InteractiveUserName
  )

  $pluginName = 'com.deskflow.control.sdPlugin'
  $pluginSrc = Join-Path $InstallPath "StreamDeck\$pluginName"
  if (-not (Test-Path (Join-Path $pluginSrc 'manifest.json'))) {
    Write-Host "No Stream Deck plugin in package (skipped)." -ForegroundColor DarkGray
    return
  }

  # Prefer the interactive Stream Deck desktop user (micha on the server), then
  # the currently logged-on interactive account.
  $targets = [System.Collections.Generic.List[string]]::new()
  foreach ($candidate in @('micha', $InteractiveUserName)) {
    if ([string]::IsNullOrWhiteSpace($candidate)) { continue }
    $short = if ($candidate -match '\\') { ($candidate -split '\\')[-1] } else { $candidate }
    if (-not $targets.Contains($short)) { $targets.Add($short) }
  }

  $installedAny = $false
  foreach ($short in $targets) {
    $profilePath = Get-UserProfilePathForName -UserName $short
    if (-not $profilePath) {
      Write-Host "Stream Deck: no profile for user '$short' (skipped)." -ForegroundColor DarkGray
      continue
    }
    $destRoot = Join-Path $profilePath 'AppData\Roaming\Elgato\StreamDeck\Plugins'
    $dest = Join-Path $destRoot $pluginName
    New-Item -ItemType Directory -Path $destRoot -Force | Out-Null

    # Stop Stream Deck briefly so plugin files are not locked, then restart if it was running.
    $sdProcs = @(Get-Process -Name 'StreamDeck' -ErrorAction SilentlyContinue |
      Where-Object { $_.Path -and $_.Path -like '*\Elgato\StreamDeck\*' })
    $wasRunning = $sdProcs.Count -gt 0
    $sdExe = $null
    if ($wasRunning) {
      $sdExe = $sdProcs[0].Path
      Write-Host "Stopping Stream Deck to update plugin for $short..."
      foreach ($p in $sdProcs) {
        try { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } catch {}
      }
      Start-Sleep -Seconds 2
    }

    if (Test-Path $dest) {
      Remove-Item $dest -Recurse -Force -ErrorAction SilentlyContinue
    }
    robocopy $pluginSrc $dest /E /R:2 /W:2 /NFL /NDL /NJH /NJS /NC /NS | Out-Null
    if ($LASTEXITCODE -ge 8) {
      Write-Host "Stream Deck plugin copy failed for $short (robocopy exit $LASTEXITCODE)." -ForegroundColor Yellow
      continue
    }
    Write-Host "Installed Stream Deck plugin for ${short}: $dest" -ForegroundColor Green
    $installedAny = $true

    if ($wasRunning -and $sdExe -and (Test-Path $sdExe)) {
      $sdUser = if ($InteractiveUserName) { $InteractiveUserName } else { $short }
      $bootstrap = 'DeskflowStreamDeckRestart'
      Unregister-ScheduledTask -TaskName $bootstrap -Confirm:$false -ErrorAction SilentlyContinue
      try {
        $action = New-ScheduledTaskAction -Execute $sdExe
        $principal = New-ScheduledTaskPrincipal -UserId $sdUser -LogonType Interactive -RunLevel Limited
        $trigger = New-ScheduledTaskTrigger -Once -At (Get-Date).AddMinutes(-1)
        $settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -StartWhenAvailable
        Register-ScheduledTask -TaskName $bootstrap -Action $action -Principal $principal -Trigger $trigger -Settings $settings -Force | Out-Null
        Start-ScheduledTask -TaskName $bootstrap -ErrorAction SilentlyContinue
        Write-Host "Restarted Stream Deck for $sdUser." -ForegroundColor Cyan
      }
      catch {
        Write-Host "Could not restart Stream Deck automatically: $($_.Exception.Message)" -ForegroundColor Yellow
        Write-Host "Start Stream Deck manually to load the Deskflow plugin." -ForegroundColor Yellow
      }
      finally {
        Start-Sleep -Seconds 2
        Unregister-ScheduledTask -TaskName $bootstrap -Confirm:$false -ErrorAction SilentlyContinue
      }
    }
  }

  if ($installedAny) {
    Write-Host "Add the Deskflow -> Pause action to a Stream Deck key once (Software UI)." -ForegroundColor Cyan
  }
}

$payload = Join-Path $PSScriptRoot 'Deskflow'
if (-not (Test-Path (Join-Path $payload 'deskflow-core.exe'))) {
  throw "Deskflow payload not found next to this script. Extract the full portable zip first."
}

Write-Host "==> Installing to $InstallPath" -ForegroundColor Cyan

if (-not (Test-Path $InstallPath)) {
  New-Item -ItemType Directory -Path $InstallPath -Force | Out-Null
}

function Stop-DeskflowBinariesForUpdate {
  # Must stop GUI/core BEFORE robocopy or Windows keeps the locked deskflow.exe
  # and Install-Remote silently leaves the previous UI in place.
  $svcLocal = Get-Service -Name 'Deskflow' -ErrorAction SilentlyContinue
  if ($svcLocal -and $svcLocal.Status -ne 'Stopped') {
    Write-Host "Stopping Deskflow service..."
    Stop-Service -Name 'Deskflow' -Force -ErrorAction SilentlyContinue
  }

  $names = @('deskflow', 'deskflow-ui', 'deskflow-core', 'deskflow-daemon')
  for ($attempt = 1; $attempt -le 5; $attempt++) {
    $procs = @(Get-Process -Name $names -ErrorAction SilentlyContinue)
    if ($procs.Count -eq 0) { break }
    Write-Host ("Stopping Deskflow processes (attempt {0}): {1}" -f $attempt, (
      ($procs | ForEach-Object { $_.ProcessName }) -join ', '
    ))
    foreach ($p in $procs) {
      try { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } catch {}
    }
    Start-Sleep -Milliseconds 600
  }

  $left = @(Get-Process -Name $names -ErrorAction SilentlyContinue)
  if ($left.Count -gt 0) {
    throw ("Cannot update while still running: {0}. Close Deskflow and retry." -f (
      ($left | ForEach-Object { "{0}(pid={1})" -f $_.ProcessName, $_.Id }) -join ', '
    ))
  }
}

Stop-DeskflowBinariesForUpdate

$payloadGui = Join-Path $payload 'deskflow.exe'
$payloadMarker = Join-Path $payload 'deskflow-ui.json'
$payloadHash = (Get-FileHash -Path $payloadGui -Algorithm SHA256).Hash

robocopy $payload $InstallPath /MIR /R:2 /W:2 /NFL /NDL /NJH /NJS /NC /NS | Out-Null
if ($LASTEXITCODE -ge 8) {
  throw "Install copy failed (robocopy exit $LASTEXITCODE)."
}

$daemon = Join-Path $InstallPath 'deskflow-daemon.exe'
$gui = Join-Path $InstallPath 'deskflow.exe'
$coreExe = Join-Path $InstallPath 'deskflow-core.exe'

# Prove the GUI binary was actually replaced (not skipped due to a file lock).
if (-not (Test-Path $gui)) {
  throw "Install incomplete: deskflow.exe missing under $InstallPath"
}
$installedHash = (Get-FileHash -Path $gui -Algorithm SHA256).Hash
if ($installedHash -ne $payloadHash) {
  throw ("Install incomplete: deskflow.exe was not updated (likely still locked).`n" +
    "  Payload:    $payloadHash`n" +
    "  Installed:  $installedHash`n" +
    "Close Deskflow completely and re-run Install-Remote.ps1.")
}
if (Test-Path $payloadMarker) {
  Copy-Item $payloadMarker (Join-Path $InstallPath 'deskflow-ui.json') -Force
  try {
    $marker = Get-Content (Join-Path $InstallPath 'deskflow-ui.json') -Raw | ConvertFrom-Json
    if ($marker.versionId) {
      Write-Host "Installed GUI version: $($marker.versionId)" -ForegroundColor Green
    }
  } catch {}
}
Write-Host "GUI binary verified (SHA256 match)." -ForegroundColor Green

$cliInstalled = Join-Path $InstallPath 'deskflow-cli.exe'
if (Test-Path $cliInstalled) {
  Write-Host "deskflow-cli.exe present." -ForegroundColor Green
}
else {
  Write-Host "WARNING: deskflow-cli.exe missing (Stream Deck Pause will not work)." -ForegroundColor Yellow
}

# Confirm core/daemon were replaced with the same build number as the GUI marker.
function Get-InstallVersionNumberPart([string] $VersionId) {
  if (-not $VersionId) { return $null }
  $v = $VersionId.Trim()
  if ($v.StartsWith('v')) { $v = $v.Substring(1) }
  $plus = $v.IndexOf('+')
  if ($plus -ge 0) { $v = $v.Substring(0, $plus) }
  $space = $v.IndexOf(' ')
  if ($space -ge 0) { $v = $v.Substring(0, $space) }
  return $v
}
$expectedGui = $null
if (Test-Path (Join-Path $InstallPath 'deskflow-ui.json')) {
  try {
    $expectedGui = (Get-Content (Join-Path $InstallPath 'deskflow-ui.json') -Raw | ConvertFrom-Json).versionId
  } catch {}
}
$coreOut = & $coreExe --version 2>&1 | Out-String
$daemonOutFile = [System.IO.Path]::GetTempFileName()
$daemonErrFile = [System.IO.Path]::GetTempFileName()
try {
  Start-Process -FilePath $daemon -ArgumentList '--version' -Wait -NoNewWindow `
    -RedirectStandardOutput $daemonOutFile -RedirectStandardError $daemonErrFile | Out-Null
  $daemonOut = ((Get-Content -LiteralPath $daemonOutFile -Raw -ErrorAction SilentlyContinue) +
    (Get-Content -LiteralPath $daemonErrFile -Raw -ErrorAction SilentlyContinue))
}
finally {
  Remove-Item -LiteralPath $daemonOutFile,$daemonErrFile -Force -ErrorAction SilentlyContinue
}
$coreVer = $null
$daemonVer = $null
if ($coreOut -match 'v(\d+\.\d+\.\d+(?:\.\d+)?)\s+\(([0-9a-fA-F]+)\)') {
  $coreVer = "$($Matches[1])+$($Matches[2])"
} elseif ($coreOut -match '(\d+\.\d+\.\d+(?:\.\d+)?)') {
  $coreVer = $Matches[1]
}
if ($daemonOut -match '(\d+\.\d+\.\d+(?:\.\d+)?)') {
  if ($daemonOut -match '(\d+\.\d+\.\d+(?:\.\d+)?)\+([0-9a-fA-F]+)') {
    $daemonVer = "$($Matches[1])+$($Matches[2])"
  } else {
    $daemonVer = $Matches[1]
  }
}
$guiNum = Get-InstallVersionNumberPart $expectedGui
$coreNum = Get-InstallVersionNumberPart $coreVer
$daemonNum = Get-InstallVersionNumberPart $daemonVer
Write-Host "Installed versions: GUI=$expectedGui core=$coreVer daemon=$daemonVer"
if ($expectedGui -and $coreVer -and $daemonVer -and ($coreVer -ne $expectedGui -or $daemonVer -ne $expectedGui)) {
  throw ("Install version skew: GUI=$expectedGui core=$coreVer daemon=$daemonVer.`n" +
    "Close all Deskflow processes and re-run Install-Remote.ps1 from a fresh portable package.")
}
if ($expectedGui -and $coreVer -and $daemonVer) {
  Write-Host "GUI/core/daemon exact versions match ($expectedGui)." -ForegroundColor Green
}

$user = Get-InteractiveUserName
$forceDesktopMode = $Desktop -or $NoService
$mode = Resolve-DeskflowInstallMode -UserName $user -ForceService:$UseService -ForceDesktop:$forceDesktopMode
$useServiceMode = ($mode -eq 'Service')
$svc = Get-Service -Name 'Deskflow' -ErrorAction SilentlyContinue

# Always open inbound TCP 24800. Server installs that only listen without this
# rule leave remote clients stuck on "failed to connect: Timed out" / SynSent.
Write-Host "Ensuring Windows Firewall allows inbound Deskflow (TCP 24800)..." -ForegroundColor Cyan
Ensure-DeskflowFirewallRules -CorePath $coreExe -Port 24800

if (-not $NoService) {
  $startup = if ($useServiceMode) { 'Automatic' } else { 'Manual' }
  if (-not $svc) {
    Write-Host "Creating Deskflow service (startup=$startup)..."
    New-Service -Name 'Deskflow' -BinaryPathName "`"$daemon`"" -DisplayName 'Deskflow' -StartupType $startup | Out-Null
  }
  else {
    $scStart = if ($useServiceMode) { 'auto' } else { 'demand' }
    sc.exe config Deskflow binPath= "`"$daemon`"" start= $scStart obj= LocalSystem | Out-Null
  }
  sc.exe description Deskflow "Runs the Core process on secure desktops (UAC prompts, login screen, etc)." | Out-Null
  sc.exe failure Deskflow reset= 86400 actions= restart/1000/restart/1000/none/0 | Out-Null
  sc.exe failureflag Deskflow 1 | Out-Null
}

Write-Host ""
Write-Host "Prerequisites (install once on this PC if missing):" -ForegroundColor Yellow
Write-Host "  - Visual C++ Redistributable 2015-2022 x64:"
Write-Host "    https://aka.ms/vs/17/release/vc_redist.x64.exe"
Write-Host "  - WebView2 Runtime (Tauri GUI / tray):"
Write-Host "    https://developer.microsoft.com/microsoft-edge/webview2/"
Write-Host ""

function Start-DeskflowServiceStack {
  Set-DeskflowProcessMode -Mode Service -UserName $user
  if ($NoService -or $NoStart) {
    Write-Host "Service left stopped (-NoStart / -NoService)." -ForegroundColor DarkGray
    return
  }
  try {
    Start-Service -Name 'Deskflow'
    Write-Host "Deskflow service started (daemon owns core; GUI controls via IPC)." -ForegroundColor Green
    $serviceConfig = @(Get-DeskflowConfPaths -UserName $user | Where-Object {
      if (-not (Test-Path -LiteralPath $_)) { return $false }
      $coreMode = Get-DeskflowIniValue -Path $_ -Section 'core' -Key 'coreMode'
      return $coreMode -eq '1' -or $coreMode -eq '2'
    }) | Select-Object -Last 1
    if ($serviceConfig -and $expectedGui) {
      $configuredLogLevel = Get-DeskflowIniValue -Path $serviceConfig -Section 'log' -Key 'level'
      if (-not $configuredLogLevel) { $configuredLogLevel = 'INFO' }
      Invoke-DeskflowDaemonStart -VersionId $expectedGui -SettingsFile $serviceConfig -LogLevel $configuredLogLevel
    }
    else {
      Write-Host "Service is ready; core start waits for a client/server role to be saved in the GUI." -ForegroundColor Yellow
    }
  }
  catch {
    Write-Host "Service did not start: $($_.Exception.Message)" -ForegroundColor Yellow
  }
}

if ($useServiceMode) {
  Start-DeskflowServiceStack
}
else {
  # Desktop mode: GUI owns core. Do not leave a shadow daemon running.
  Set-DeskflowProcessMode -Mode Desktop -UserName $user
  $running = Get-Service -Name 'Deskflow' -ErrorAction SilentlyContinue
  if ($running -and $running.Status -eq 'Running') {
    Write-Host "Stopping Deskflow service so Desktop mode GUI owns core..."
    Stop-Service -Name 'Deskflow' -Force -ErrorAction SilentlyContinue
  }
  # Also stop orphan core started by a previous daemon watchdog.
  Get-Process -Name 'deskflow-core' -ErrorAction SilentlyContinue | ForEach-Object {
    try { Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue } catch {}
  }
  Write-Host "Desktop mode: tray GUI will start/stop deskflow-core (service registered but not running)." -ForegroundColor Green
}

if (-not $NoGui) {
  if ($user) {
    Start-DeskflowGuiForUser -GuiPath $gui -UserName $user -RegisterLogon
  }
  else {
    Write-Host "No interactive user logged on; tray GUI not launched." -ForegroundColor Yellow
    Write-Host "After someone logs in, run: `"$gui`"" -ForegroundColor Yellow
  }
}
else {
  Write-Host "Skipped tray GUI (-NoGui)." -ForegroundColor DarkGray
}

# Never leave an install that stopped core with nothing listening.
if (-not $NoStart) {
  Start-Sleep -Seconds 4
  if (-not (Test-DeskflowCoreAlive)) {
    if (-not $useServiceMode -and -not $NoService) {
      Write-Host "deskflow-core did not come up under Desktop mode; falling back to Service mode..." -ForegroundColor Yellow
      Start-DeskflowServiceStack
      Start-Sleep -Seconds 3
    }
    if (-not (Test-DeskflowCoreAlive)) {
      Write-Host "WARNING: deskflow-core is still not running. Open the tray app and press Start," -ForegroundColor Red
      Write-Host "or re-run: .\Install-Remote.ps1 -UseService" -ForegroundColor Red
    }
    else {
      Write-Host "deskflow-core is running after Service fallback." -ForegroundColor Green
    }
  }
  else {
    Write-Host "deskflow-core is running." -ForegroundColor Green
  }
}

Install-DeskflowStreamDeckPlugin -InstallPath $InstallPath -InteractiveUserName $user

Write-Host "Install complete." -ForegroundColor Green
'@

# ASCII-only script + UTF-8 BOM so Windows PowerShell 5.1 does not mis-parse
# UTF-8 em-dashes as broken strings (mojibake like "daemon a no tray").
$installPath = Join-Path $stageRoot 'Install-Remote.ps1'
$utf8Bom = New-Object System.Text.UTF8Encoding $true
[System.IO.File]::WriteAllText($installPath, $installScript, $utf8Bom)

# Standalone recovery aid: open 24800 without a full reinstall (run elevated on the SERVER).
$firewallAid = @'
#Requires -Version 5.1
#Requires -RunAsAdministrator
<#
.SYNOPSIS
  Open Windows Firewall for Deskflow server inbound TCP 24800.

.DESCRIPTION
  Use this on the SERVER when clients show "failed to connect to server: Timed out"
  / SynSent while Get-NetTCPConnection on the server shows Listen on 24800.
  Listening does not imply the firewall allows inbound traffic.
#>
[CmdletBinding()]
param(
  [string] $CorePath = "${env:ProgramFiles}\Deskflow\deskflow-core.exe",
  [int] $Port = 24800
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$portRule = 'Deskflow TCP 24800 Inbound'
if (-not (Get-NetFirewallRule -DisplayName $portRule -ErrorAction SilentlyContinue)) {
  New-NetFirewallRule -DisplayName $portRule `
    -Direction Inbound -Action Allow -Protocol TCP -LocalPort $Port `
    -Profile Any `
    -Description 'Allow remote Deskflow clients to connect to the server on TCP 24800' | Out-Null
  Write-Host "Created: $portRule" -ForegroundColor Green
}
else {
  Set-NetFirewallRule -DisplayName $portRule -Enabled True -Action Allow
  Write-Host "Enabled: $portRule" -ForegroundColor Cyan
}

if (Test-Path $CorePath) {
  foreach ($name in @('Deskflow Server', 'Deskflow Client')) {
    if (-not (Get-NetFirewallRule -DisplayName $name -ErrorAction SilentlyContinue)) {
      New-NetFirewallRule -DisplayName $name `
        -Direction Inbound -Action Allow -Program $CorePath -Profile Any | Out-Null
      Write-Host "Created: $name" -ForegroundColor Green
    }
    else {
      Set-NetFirewallRule -DisplayName $name -Enabled True -Action Allow
      Write-Host "Enabled: $name" -ForegroundColor Cyan
    }
  }
}

Write-Host ""
Write-Host "Verify from a CLIENT:" -ForegroundColor Yellow
Write-Host "  Test-NetConnection -ComputerName <server-ip> -Port $Port"
Write-Host "On this SERVER, Listen should still show:" -ForegroundColor Yellow
Write-Host "  Get-NetTCPConnection -LocalPort $Port -State Listen"
Write-Host "Done." -ForegroundColor Green
'@
$firewallAidPath = Join-Path $stageRoot 'Open-DeskflowFirewall.ps1'
[System.IO.File]::WriteAllText($firewallAidPath, $firewallAid, $utf8Bom)

$readme = @"
# Deskflow portable Windows package

Built/packaged: $stamp
Version hint: $versionHint

## What you need on the remote PC

- **No** Visual Studio / Build Tools / CMake / Qt SDK
- [Visual C++ Redistributable 2015–2022 x64](https://aka.ms/vs/17/release/vc_redist.x64.exe)
- [WebView2 Runtime](https://developer.microsoft.com/microsoft-edge/webview2/) (for the GUI / tray)

## Install

1. Copy this folder (or the .zip) to the remote machine and extract it.
2. Open **elevated** PowerShell in this folder.
3. Unblock the script (Zone.Identifier from a network copy), then run:

``````powershell
Unblock-File .\Install-Remote.ps1
Set-ExecutionPolicy -Scope Process Bypass -Force
.\Install-Remote.ps1
``````

Smart default:

1. Copies ``Deskflow\`` into ``C:\Program Files\Deskflow``
2. Opens Windows Firewall for inbound TCP **24800** (and program rules for ``deskflow-core.exe``)
3. Picks **Service** mode if this PC was already Service, is a server, or has no logged-on user
4. Otherwise uses **Desktop** mode (and sets ``startCoreWithGui=true``)
5. Verifies ``deskflow-core`` came up; if Desktop fails, falls back to Service

Always-on server / recover a dead listener:

``````powershell
.\Install-Remote.ps1 -UseService
``````

Force tray-owned core (not for headless servers):

``````powershell
.\Install-Remote.ps1 -Desktop
``````

Service only (no tray GUI):

``````powershell
.\Install-Remote.ps1 -UseService -NoGui
``````

## Clients time out while server "listens"

If a client log shows ``failed to connect to server: Timed out`` and
``Get-NetTCPConnection`` on the client is stuck in ``SynSent``, but the server
shows ``Listen`` on 24800, Windows Firewall on the **server** is almost always
dropping inbound TCP. Listening does not mean the port is reachable.

On the **server** (elevated PowerShell), either re-run install or open the port only:

``````powershell
# Full reinstall (also creates the firewall rules):
.\Install-Remote.ps1 -UseService

# Or firewall only (no file copy / service restart):
.\Open-DeskflowFirewall.ps1
``````

One-liner (no package needed) on the server:

``````powershell
New-NetFirewallRule -DisplayName 'Deskflow TCP 24800 Inbound' -Direction Inbound -Action Allow -Protocol TCP -LocalPort 24800 -Profile Any -ErrorAction SilentlyContinue; Set-NetFirewallRule -DisplayName 'Deskflow TCP 24800 Inbound' -Enabled True -Action Allow
``````

Then from the client: ``Test-NetConnection -ComputerName <server-ip> -Port 24800`` should report ``TcpTestSucceeded : True``.

## Desktop vs Service

| Mode | Who owns ``deskflow-core`` | Tray |
|------|----------------------------|------|
| Desktop | ``deskflow.exe`` (GUI) | Yes |
| Service | ``deskflow-daemon`` Windows service | Yes (GUI is control panel) |

Do not run both stacks at once (daemon started while GUI is in Desktop mode) or you get a background core the tray is not attached to.

## Contents

- ``deskflow.exe`` / ``deskflow-ui.exe`` - Tauri GUI + tray
- ``deskflow-core.exe`` - server/client core (needs Qt DLLs beside it)
- ``deskflow-daemon.exe`` - Windows service host
- ``deskflow-cli.exe`` - pause/resume/toggle/status for Stream Deck and scripts
- ``StreamDeck\com.deskflow.control.sdPlugin\`` - official Stream Deck plugin (Install-Remote copies it into the Stream Deck user Plugins folder)
- ``Qt6*.dll`` + ``plugins\`` - Qt runtime for core
- ``Open-DeskflowFirewall.ps1`` - open inbound TCP 24800 without reinstall

## Stream Deck Pause

Install-Remote sideloads the Deskflow plugin into:

``%AppData%\Elgato\StreamDeck\Plugins\com.deskflow.control.sdPlugin``

(for user ``micha`` when that profile exists, otherwise the interactive user).

After install:

1. Open the Stream Deck software
2. Drag **Deskflow -> Pause** onto a key
3. Press the key to toggle pause; the key shows Active vs Paused and stays in sync with the Deskflow GUI
"@

Set-Content -Path (Join-Path $stageRoot 'README.md') -Value $readme -Encoding UTF8

Write-Host "==> Staged files:" -ForegroundColor Cyan
Get-ChildItem $payload -Name | Select-Object -First 20 | ForEach-Object { Write-Host "    $_" }

if ($SkipZip) {
  Write-Host "Done (folder only): $stageRoot" -ForegroundColor Green
  return
}

$zipPath = Join-Path $OutDir "$pkgName.zip"
if (Test-Path $zipPath) {
  Remove-Item $zipPath -Force
}

Write-Host "==> Creating $zipPath" -ForegroundColor Cyan
Compress-Archive -Path $stageRoot -DestinationPath $zipPath -CompressionLevel Optimal

$sizeMb = [math]::Round((Get-Item $zipPath).Length / 1MB, 1)
Write-Host "Done." -ForegroundColor Green
Write-Host "  Folder: $stageRoot"
Write-Host "  Zip:    $zipPath ($sizeMb MB)"
Write-Host ""
Write-Host "Copy the zip to the remote host, extract, then run Install-Remote.ps1 as Administrator."
