#Requires -Version 5.1
<#
.SYNOPSIS
  Configure and build Deskflow on Windows (Release GUI executable).

.DESCRIPTION
  Sets up MSVC, vcpkg, and Qt (via aqt), then runs CMake + Ninja.
  Deploys Qt runtime DLLs with windeployqt. Optionally creates a portable 7z.

  Tested on Windows 11 with VS 2022 Community, CMake 4.x, Ninja (WinGet),
  Python 3.13 + aqtinstall, and Git.

.PARAMETER Configuration
  CMake build type (Release or Debug).

.PARAMETER BuildDir
  Out-of-source build directory relative to the repo root.

.PARAMETER QtVersion
  Qt version installed by aqt when Qt is missing.

.PARAMETER Jobs
  Parallel compile jobs passed to Ninja.

.PARAMETER SkipQtInstall
  Do not download Qt; require Qt6_DIR or Qt on PATH.

.PARAMETER SkipVcpkgBootstrap
  Do not clone or bootstrap vcpkg; require VCPKG_ROOT.

.PARAMETER Clean
  Remove the build directory before configuring.

.PARAMETER Package
  Run CPack after the build (7z portable archive; WiX MSI only if wix is installed).

.PARAMETER Package7zOnly
  When -Package is set, pass -G 7Z to cpack (skips WiX even if installed).

.EXAMPLE
  .\build-windows.ps1

.EXAMPLE
  .\build-windows.ps1 -Clean -Package -Package7zOnly
#>
[CmdletBinding()]
param(
  [ValidateSet('Release', 'Debug')]
  [string] $Configuration = 'Release',

  [string] $BuildDir = 'build',

  [string] $QtVersion = '6.10.3',

  [int] $Jobs = 8,

  [switch] $SkipQtInstall,
  [switch] $SkipVcpkgBootstrap,
  [switch] $Clean,
  [switch] $Package,
  [switch] $Package7zOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Write-Step([string] $Message) {
  Write-Host ""
  Write-Host "==> $Message" -ForegroundColor Cyan
}

function Resolve-RepoRoot {
  $root = $PSScriptRoot
  if (-not (Test-Path (Join-Path $root 'CMakeLists.txt'))) {
    throw "Run this script from the Deskflow repository root (expected CMakeLists.txt in $root)."
  }
  return $root
}

function Import-VcVars([string] $VcVarsPath) {
  if (-not (Test-Path $VcVarsPath)) {
    throw "vcvars64.bat not found: $VcVarsPath"
  }

  $envFile = Join-Path $env:TEMP "deskflow-vcvars-$([Guid]::NewGuid().ToString('N')).env"
  try {
    cmd /c "`"$VcVarsPath`" >nul 2>&1 && set" | Out-File -FilePath $envFile -Encoding ascii
    Get-Content $envFile | ForEach-Object {
      if ($_ -match '^([^=]+)=(.*)$') {
        Set-Item -Path "env:$($matches[1])" -Value $matches[2]
      }
    }
  }
  finally {
    Remove-Item $envFile -Force -ErrorAction SilentlyContinue
  }

  if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    throw "MSVC compiler (cl.exe) not available after importing $VcVarsPath"
  }
}

function Find-VcVarsPath {
  $candidates = @(
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
    "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
  )

  foreach ($path in $candidates) {
    if (Test-Path $path) {
      return $path
    }
  }

  throw "Visual Studio 2022 (or Build Tools) with the C++ workload was not found."
}

function Find-AqtExe {
  if ($env:AQT_EXE -and (Test-Path $env:AQT_EXE)) {
    return $env:AQT_EXE
  }

  $fromPath = Get-Command aqt.exe -ErrorAction SilentlyContinue
  if ($fromPath) {
    return $fromPath.Source
  }

  $storeRoot = Join-Path $env:LOCALAPPDATA 'Packages'
  if (Test-Path $storeRoot) {
    $found = Get-ChildItem -Path $storeRoot -Filter aqt.exe -Recurse -ErrorAction SilentlyContinue |
      Select-Object -First 1
    if ($found) {
      return $found.FullName
    }
  }

  $pipRoot = Join-Path $env:LOCALAPPDATA 'Programs\Python'
  if (Test-Path $pipRoot) {
    $found = Get-ChildItem -Path $pipRoot -Filter aqt.exe -Recurse -ErrorAction SilentlyContinue |
      Select-Object -First 1
    if ($found) {
      return $found.FullName
    }
  }

  throw "aqt.exe not found. Install with: pip install aqtinstall"
}

function Ensure-ToolOnPath([string] $Name) {
  $cmd = Get-Command $Name -ErrorAction SilentlyContinue
  if (-not $cmd) {
    throw "Required tool '$Name' was not found on PATH."
  }
  return $cmd.Source
}

function Prepend-Path([string] $Directory) {
  if (Test-Path $Directory) {
    $env:Path = "$Directory;$env:Path"
  }
}

function Read-VcpkgBaseline([string] $RepoRoot) {
  $manifest = Join-Path $RepoRoot 'vcpkg.json'
  if (-not (Test-Path $manifest)) {
    throw "Missing vcpkg manifest: $manifest"
  }

  $json = Get-Content $manifest -Raw | ConvertFrom-Json
  if (-not $json.'builtin-baseline') {
    throw "vcpkg.json does not contain builtin-baseline."
  }
  return [string]$json.'builtin-baseline'
}

function Ensure-Vcpkg([string] $RepoRoot) {
  $localVcpkg = Join-Path $RepoRoot 'vcpkg'
  $localExe = Join-Path $localVcpkg 'vcpkg.exe'

  if (Test-Path $localExe) {
    $vcpkgRoot = $localVcpkg
  }
  elseif ($env:VCPKG_ROOT -and (Test-Path (Join-Path $env:VCPKG_ROOT 'vcpkg.exe'))) {
    $vcpkgRoot = $env:VCPKG_ROOT
  }
  else {
    $vcpkgRoot = $localVcpkg
  }

  $env:VCPKG_ROOT = $vcpkgRoot
  $vcpkgExe = Join-Path $vcpkgRoot 'vcpkg.exe'
  if (-not (Test-Path $vcpkgExe)) {
    if ($SkipVcpkgBootstrap) {
      throw "vcpkg.exe not found at $vcpkgExe and -SkipVcpkgBootstrap was set."
    }

    Write-Step "Cloning and bootstrapping vcpkg"
    if (-not (Test-Path $vcpkgRoot)) {
      git clone https://github.com/microsoft/vcpkg.git $vcpkgRoot
    }
    & (Join-Path $vcpkgRoot 'bootstrap-vcpkg.bat') -disableMetrics
    if ($LASTEXITCODE -ne 0) {
      throw "vcpkg bootstrap failed."
    }
  }

  $baseline = Read-VcpkgBaseline $RepoRoot
  Write-Step "Ensuring vcpkg baseline $baseline"

  if (-not (Test-Path (Join-Path $vcpkgRoot '.git'))) {
    Write-Host "Skipping baseline checkout (vcpkg at $vcpkgRoot is not a git clone)."
    return $vcpkgRoot
  }

  $currentHead = (& git -C $vcpkgRoot rev-parse HEAD 2>$null)
  if ($LASTEXITCODE -eq 0 -and $currentHead.Trim() -eq $baseline) {
    Write-Host "vcpkg already at baseline $baseline"
    return $vcpkgRoot
  }

  git -C $vcpkgRoot fetch --depth 1 origin $baseline
  if ($LASTEXITCODE -ne 0) {
    throw "git fetch for vcpkg baseline failed."
  }
  git -C $vcpkgRoot checkout $baseline
  if ($LASTEXITCODE -ne 0) {
    throw "git checkout for vcpkg baseline failed."
  }

  return $vcpkgRoot
}

function Get-QtInstallDir([string] $RepoRoot, [string] $Version) {
  return Join-Path $RepoRoot ".qt\$Version\msvc2022_64"
}

function Ensure-Qt([string] $RepoRoot, [string] $Version) {
  $qtDir = Get-QtInstallDir $RepoRoot $Version
  $qt6Cmake = Join-Path $qtDir 'lib\cmake\Qt6\Qt6Config.cmake'

  if (Test-Path $qt6Cmake) {
    Write-Step "Using existing Qt at $qtDir"
    return $qtDir
  }

  if ($SkipQtInstall) {
    if ($env:Qt6_DIR) {
      return (Split-Path (Split-Path $env:Qt6_DIR -Parent) -Parent)
    }
    throw "Qt $Version is not installed at $qtDir and -SkipQtInstall was set."
  }

  $aqt = Find-AqtExe
  Write-Step "Installing Qt $Version with aqt"
  & $aqt install-qt windows desktop $Version win64_msvc2022_64 `
    --outputdir (Join-Path $RepoRoot '.qt') `
    --base `
    --archives qtbase qtsvg qttools qttranslations
  if ($LASTEXITCODE -ne 0) {
    throw "aqt install-qt failed."
  }

  if (-not (Test-Path $qt6Cmake)) {
    throw "Qt install finished but Qt6Config.cmake was not found at $qt6Cmake"
  }

  return $qtDir
}

function Invoke-CMakeConfigure {
  param(
    [string] $RepoRoot,
    [string] $BuildPath,
    [string] $VcpkgRoot,
    [string] $QtDir,
    [string] $Config
  )

  $qt6Dir = Join-Path $QtDir 'lib\cmake\Qt6'
  $toolchain = Join-Path $VcpkgRoot 'scripts\buildsystems\vcpkg.cmake'

  $args = @(
    '-S', $RepoRoot,
    '-B', $BuildPath,
    '-G', 'Ninja',
    "-DCMAKE_BUILD_TYPE=$Config",
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain",
    '-DVCPKG_TARGET_TRIPLET=x64-windows-release',
    "-DQt6_DIR=$qt6Dir",
    '-DSKIP_BUILD_TESTS=ON',
    '-DCMAKE_COMPILE_WARNING_AS_ERROR=OFF'
  )

  Write-Step "Configuring CMake"
  & cmake @args
  if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed."
  }
}

function Invoke-Build([string] $BuildPath, [string] $Config, [int] $JobCount) {
  Write-Step "Building Deskflow ($Config, $JobCount jobs)"
  & cmake --build $BuildPath --config $Config -j $JobCount
  if ($LASTEXITCODE -ne 0) {
    throw "Build failed."
  }
}

function Invoke-Deploy([string] $BuildPath, [string] $QtDir) {
  $binDir = Join-Path $BuildPath 'bin'
  $deskflowExe = Join-Path $binDir 'deskflow.exe'
  if (-not (Test-Path $deskflowExe)) {
    throw "Expected executable not found: $deskflowExe"
  }

  $windeployqt = Join-Path $QtDir 'bin\windeployqt.exe'
  if (-not (Test-Path $windeployqt)) {
    throw "windeployqt not found: $windeployqt"
  }

  Write-Step "Deploying Qt runtime with windeployqt"
  & $windeployqt --no-translations $deskflowExe | Out-Host
  if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed."
  }

  return $deskflowExe
}

function Invoke-Package([string] $BuildPath, [switch] $Only7z) {
  Write-Step "Creating package"
  Push-Location $BuildPath
  try {
    if ($Only7z) {
      & cpack -G 7Z --config ./CPackConfig.cmake | Out-Host
    }
    else {
      & cpack --config ./CPackConfig.cmake | Out-Host
    }
    if ($LASTEXITCODE -ne 0) {
      throw "CPack failed."
    }

    $archives = @(
      Get-ChildItem -Path $BuildPath -Filter '*.7z' -File
      Get-ChildItem -Path $BuildPath -Filter '*.msi' -File
    )
    return ,$archives
  }
  finally {
    Pop-Location
  }
}

# --- main ---

$repoRoot = Resolve-RepoRoot
$buildPath = Join-Path $repoRoot $BuildDir

Write-Step "Importing MSVC environment"
Import-VcVars (Find-VcVarsPath)

Ensure-ToolOnPath 'cmake' | Out-Null
Ensure-ToolOnPath 'git' | Out-Null
$ninjaPath = Ensure-ToolOnPath 'ninja'

$vcpkgRoot = Ensure-Vcpkg $repoRoot
$qtDir = Ensure-Qt $repoRoot $QtVersion

Prepend-Path (Split-Path $ninjaPath -Parent)
Prepend-Path (Join-Path $qtDir 'bin')

if ($Clean -and (Test-Path $buildPath)) {
  Write-Step "Removing build directory $buildPath"
  Remove-Item $buildPath -Recurse -Force
}

Invoke-CMakeConfigure -RepoRoot $repoRoot -BuildPath $buildPath -VcpkgRoot $vcpkgRoot -QtDir $qtDir -Config $Configuration
Invoke-Build -BuildPath $buildPath -Config $Configuration -JobCount $Jobs
$exePath = Invoke-Deploy -BuildPath $buildPath -QtDir $qtDir

$packages = @()
if ($Package) {
  $packages = @(Invoke-Package -BuildPath $buildPath -Only7z:$Package7zOnly)
}

Write-Host ""
Write-Host "Build complete." -ForegroundColor Green
Write-Host "  GUI:       $exePath"
Write-Host "  Core:      $(Join-Path $buildPath 'bin\deskflow-core.exe')"
Write-Host "  Run:       & `"$exePath`" --version"

if ($packages.Count -gt 0) {
  Write-Host "  Packages:"
  foreach ($pkg in $packages) {
    Write-Host "    $($pkg.FullName)"
  }
}
elseif ($Package) {
  Write-Host "  Package:   (no archives found under $buildPath)"
}
