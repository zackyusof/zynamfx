# Packaging script for zynamfx Windows releases
# Run from repository root: powershell -ExecutionPolicy Bypass -File script/package-windows-release.ps1

param (
    [string]$ArtefactsDir = "build/plugin/TONE3000_artefacts/Release",
    [string]$OutputDir = "release",
    [switch]$SkipInstaller = $false
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot

# Read version
$VersionFile = Join-Path $RepoRoot "VERSION"
if (Test-Path $VersionFile) {
    $Version = (Get-Content $VersionFile).Trim()
} else {
    $Version = "2.2.0"
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " Packaging zynamfx v$Version (Windows x64)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$AbsArtefactsDir = Resolve-Path (Join-Path $RepoRoot $ArtefactsDir) -ErrorAction SilentlyContinue
if (-not $AbsArtefactsDir -or -not (Test-Path $AbsArtefactsDir)) {
    Write-Error "Artefacts directory not found: $ArtefactsDir. Please build the project first."
}

$AbsOutputDir = Join-Path $RepoRoot $OutputDir
if (-not (Test-Path $AbsOutputDir)) {
    New-Item -ItemType Directory -Path $AbsOutputDir -Force | Out-Null
}

$TempStage = Join-Path $AbsOutputDir "staging_temp"
if (Test-Path $TempStage) {
    Remove-Item -Recurse -Force $TempStage
}
New-Item -ItemType Directory -Path $TempStage -Force | Out-Null

try {
    # 1. Package Standalone
    $StandaloneExe = Join-Path $AbsArtefactsDir "Standalone\zynamfx.exe"
    if (Test-Path $StandaloneExe) {
        Write-Host "Packaging Standalone..." -ForegroundColor Green
        $StageStandalone = Join-Path $TempStage "standalone"
        New-Item -ItemType Directory -Path $StageStandalone -Force | Out-Null
        Copy-Item $StandaloneExe -Destination $StageStandalone

        $ReadmeContent = @"
zynamfx v$Version - Standalone (Windows x64)
============================================

Quick Start:
1. Double click zynamfx.exe to run.
2. Open Settings (gear icon) -> Audio to select your audio device.
   - For lowest latency, select your Audio Interface's native ASIO driver.
3. Plug in your guitar, load tones, or load your own .nam / IR files!

For issues and updates:
https://github.com/zackyusof/zynamfx
"@
        Set-Content -Path (Join-Path $StageStandalone "README.txt") -Value $ReadmeContent

        $StandaloneZip = Join-Path $AbsOutputDir "zynamfx-$Version-windows-x64-standalone.zip"
        if (Test-Path $StandaloneZip) { Remove-Item -Force $StandaloneZip }
        Compress-Archive -Path "$StageStandalone\*" -DestinationPath $StandaloneZip
        Write-Host " -> Created $StandaloneZip" -ForegroundColor Yellow
    } else {
        Write-Warning "Standalone executable not found at $StandaloneExe"
    }

    # 2. Package VST3
    $Vst3Dir = Join-Path $AbsArtefactsDir "VST3\zynamfx.vst3"
    if (Test-Path $Vst3Dir) {
        Write-Host "Packaging VST3..." -ForegroundColor Green
        $StageVst3 = Join-Path $TempStage "vst3"
        New-Item -ItemType Directory -Path $StageVst3 -Force | Out-Null
        Copy-Item -Recurse $Vst3Dir -Destination $StageVst3

        $Vst3Readme = @"
zynamfx v$Version - VST3 Plugin (Windows x64)
=============================================

Installation:
1. Copy the entire 'zynamfx.vst3' directory into your standard Windows VST3 folder:
   C:\Program Files\Common Files\VST3\

2. Rescan plugins in your DAW (Reaper, Cubase, Ableton, FL Studio, Studio One, etc.).

3. Insert 'zynamfx' on your audio track.
"@
        Set-Content -Path (Join-Path $StageVst3 "INSTALL.txt") -Value $Vst3Readme

        $Vst3Zip = Join-Path $AbsOutputDir "zynamfx-$Version-windows-x64-vst3.zip"
        if (Test-Path $Vst3Zip) { Remove-Item -Force $Vst3Zip }
        Compress-Archive -Path "$StageVst3\*" -DestinationPath $Vst3Zip
        Write-Host " -> Created $Vst3Zip" -ForegroundColor Yellow
    } else {
        Write-Warning "VST3 bundle not found at $Vst3Dir"
    }

    # 3. Package Complete Bundle (Standalone + VST3 + CLAP + Factory Presets)
    Write-Host "Packaging Complete Bundle..." -ForegroundColor Green
    $StageComplete = Join-Path $TempStage "complete"
    New-Item -ItemType Directory -Path $StageComplete -Force | Out-Null

    if (Test-Path $StandaloneExe) {
        $CompleteStandaloneDir = Join-Path $StageComplete "Standalone"
        New-Item -ItemType Directory -Path $CompleteStandaloneDir -Force | Out-Null
        Copy-Item $StandaloneExe -Destination $CompleteStandaloneDir
    }
    if (Test-Path $Vst3Dir) {
        Copy-Item -Recurse $Vst3Dir -Destination $StageComplete
    }
    $ClapFile = Join-Path $AbsArtefactsDir "CLAP\zynamfx.clap"
    if (Test-Path $ClapFile) {
        Copy-Item $ClapFile -Destination $StageComplete
    }
    $PresetsSrc = Join-Path $RepoRoot "resources\factory-presets"
    if (Test-Path $PresetsSrc) {
        $CompletePresetsDir = Join-Path $StageComplete "Factory Presets"
        New-Item -ItemType Directory -Path $CompletePresetsDir -Force | Out-Null
        Copy-Item (Join-Path $PresetsSrc "*.t3kpreset") -Destination $CompletePresetsDir
    }

    # Add install.cmd and uninstall.cmd for one-click setup from the ZIP bundle
    $InstallCmdContent = @"
@echo off
setlocal EnableDelayedExpansion
title zynamfx Installer

echo ====================================================
echo  zynamfx v$Version - Windows Quick Installer
echo ====================================================
echo.

:: Check for administrative rights
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [Notice] Administrator permissions are required to install plugins
    echo          into Common Files and ProgramData.
    echo.
    echo Requesting elevation...
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

cd /d "%~dp0"

:: 1. VST3 Installation
if exist "zynamfx.vst3" (
    echo [1/3] Installing VST3 plugin...
    set "VST3_DIR=%CommonProgramFiles%\VST3"
    if not exist "!VST3_DIR!" mkdir "!VST3_DIR!"
    if exist "!VST3_DIR!\zynamfx.vst3" rmdir /s /q "!VST3_DIR!\zynamfx.vst3"
    xcopy /E /I /Q /Y "zynamfx.vst3" "!VST3_DIR!\zynamfx.vst3" >nul
    echo       Installed to: !VST3_DIR!\zynamfx.vst3
) else (
    echo [1/3] Skipping VST3 (not found)
)

:: 2. CLAP Installation
if exist "zynamfx.clap" (
    echo [2/3] Installing CLAP plugin...
    set "CLAP_DIR=%CommonProgramFiles%\CLAP"
    if not exist "!CLAP_DIR!" mkdir "!CLAP_DIR!"
    copy /Y "zynamfx.clap" "!CLAP_DIR!\" >nul
    echo       Installed to: !CLAP_DIR!\zynamfx.clap
) else (
    echo [2/3] Skipping CLAP (not present)
)

:: 3. Factory Presets
if exist "Factory Presets" (
    echo [3/3] Installing Factory Presets...
    set "PRESET_DIR=%ProgramData%\zynamfx\Presets\Factory"
    if not exist "!PRESET_DIR!" mkdir "!PRESET_DIR!"
    xcopy /E /I /Q /Y "Factory Presets\*.t3kpreset" "!PRESET_DIR!\" >nul
    echo       Installed to: !PRESET_DIR!
) else (
    echo [3/3] Skipping Presets (not found)
)

echo.
echo ====================================================
echo  Installation successful!
echo ====================================================
echo - Standalone can be run directly from the 'Standalone' folder.
echo - In your DAW (Reaper, Cubase, Ableton, FL Studio, etc.),
echo   run a plugin rescan to load 'zynamfx'.
echo.
pause
"@
    Set-Content -Path (Join-Path $StageComplete "install.cmd") -Value $InstallCmdContent

    $UninstallCmdContent = @"
@echo off
setlocal
title zynamfx Uninstaller

echo ====================================================
echo  zynamfx - Quick Uninstaller
echo ====================================================
echo.

net session >nul 2>&1
if %errorlevel% neq 0 (
    echo Requesting elevation...
    powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

if exist "%CommonProgramFiles%\VST3\zynamfx.vst3" (
    rmdir /s /q "%CommonProgramFiles%\VST3\zynamfx.vst3"
    echo Removed %CommonProgramFiles%\VST3\zynamfx.vst3
)

if exist "%CommonProgramFiles%\CLAP\zynamfx.clap" (
    del /f /q "%CommonProgramFiles%\CLAP\zynamfx.clap"
    echo Removed %CommonProgramFiles%\CLAP\zynamfx.clap
)

echo.
echo Uninstallation complete!
pause
"@
    Set-Content -Path (Join-Path $StageComplete "uninstall.cmd") -Value $UninstallCmdContent

    $CompleteReadme = @"
zynamfx v$Version - Complete Windows x64 Bundle
===============================================

Quick Install:
- Double click 'install.cmd' to automatically copy VST3, CLAP and Presets
  to their standard Windows system locations (works on all language editions).
- To uninstall, double click 'uninstall.cmd'.

Included components:
- install.cmd            : Automated 1-click installer script
- uninstall.cmd          : Automated uninstaller script
- Standalone\zynamfx.exe : Standalone application (with ASIO audio driver support)
- zynamfx.vst3           : VST3 plugin (destination: %CommonProgramFiles%\VST3\)
- zynamfx.clap           : CLAP plugin (destination: %CommonProgramFiles%\CLAP\)
- Factory Presets\       : Shipped factory presets (.t3kpreset)
                           destination: %ProgramData%\zynamfx\Presets\Factory\

License: MIT
Repository: https://github.com/zackyusof/zynamfx
"@
    Set-Content -Path (Join-Path $StageComplete "README.txt") -Value $CompleteReadme

    $CompleteZip = Join-Path $AbsOutputDir "zynamfx-$Version-windows-x64-complete.zip"
    if (Test-Path $CompleteZip) { Remove-Item -Force $CompleteZip }
    Compress-Archive -Path "$StageComplete\*" -DestinationPath $CompleteZip
    Write-Host " -> Created $CompleteZip" -ForegroundColor Yellow

    # 4. Inno Setup Compiler (.exe Installer) if available
    if (-not $SkipInstaller) {
        $IsccCmd = Get-Command "iscc" -ErrorAction SilentlyContinue
        $IsccPath = ""
        if ($IsccCmd) {
            $IsccPath = $IsccCmd.Source
        } elseif (Test-Path "C:\Program Files (x86)\Inno Setup 6\ISCC.exe") {
            $IsccPath = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
        } elseif (Test-Path "C:\Program Files\Inno Setup 6\ISCC.exe") {
            $IsccPath = "C:\Program Files\Inno Setup 6\ISCC.exe"
        }

        if ($IsccPath) {
            Write-Host "Compiling Inno Setup installer using $IsccPath..." -ForegroundColor Green
            $IssFile = Join-Path $RepoRoot "script\installer\windows\zynamfx.iss"
            & $IsccPath "/DVersion=$Version" "/DArtefactsDir=$AbsArtefactsDir" "/DOutputDir=$AbsOutputDir" $IssFile
            Write-Host " -> Installer compiled successfully!" -ForegroundColor Yellow
        } else {
            Write-Host "Note: Inno Setup (ISCC) not found on PATH. Skipped installer creation (zip packages ready)." -ForegroundColor Cyan
        }
    }

} finally {
    if (Test-Path $TempStage) {
        Remove-Item -Recurse -Force $TempStage
    }
}

Write-Host "`nRelease files created in ${AbsOutputDir}:" -ForegroundColor Green
Get-ChildItem -Path $AbsOutputDir -Filter "zynamfx-$Version-windows-x64-*" | ForEach-Object {
    $Hash = (Get-FileHash $_.FullName -Algorithm SHA256).Hash
    Write-Host ("  - {0,-45} ({1:N2} MB)  SHA256: {2}" -f $_.Name, ($_.Length / 1MB), $Hash.Substring(0, 16))
}
Write-Host "Done!" -ForegroundColor Green
