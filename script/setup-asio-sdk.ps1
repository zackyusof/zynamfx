# Downloads and extracts the Steinberg ASIO SDK into libs/asiosdk
# Required for building the standalone application with ASIO support on Windows.

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = (Get-Item "$ScriptDir\..").FullName
$TargetDir = "$RepoRoot\libs\asiosdk"
$ExpectedHeader = "$TargetDir\ASIOSDK\common\iasiodrv.h"

if (Test-Path $ExpectedHeader) {
    Write-Host "ASIO SDK already installed at: $TargetDir\ASIOSDK" -ForegroundColor Green
    exit 0
}

Write-Host "Downloading Steinberg ASIO SDK..." -ForegroundColor Cyan
$ZipPath = "$RepoRoot\libs\asiosdk-download.zip"
New-Item -ItemType Directory -Path "$RepoRoot\libs" -Force | Out-Null

try {
    # Steinberg direct download endpoint for ASIO SDK
    $AsioSdkUrl = "https://www.steinberg.net/asiosdk"
    Invoke-WebRequest -Uri $AsioSdkUrl -OutFile $ZipPath -UseBasicParsing
    
    Write-Host "Extracting ASIO SDK to $TargetDir..." -ForegroundColor Cyan
    if (Test-Path $TargetDir) {
        Remove-Item -Path $TargetDir -Recurse -Force
    }
    Expand-Archive -Path $ZipPath -DestinationPath $TargetDir -Force
}
finally {
    if (Test-Path $ZipPath) {
        Remove-Item -Path $ZipPath -Force
    }
}

if (Test-Path $ExpectedHeader) {
    Write-Host "Successfully installed Steinberg ASIO SDK to $TargetDir\ASIOSDK" -ForegroundColor Green
} else {
    Write-Error "ASIO SDK extraction completed, but $ExpectedHeader was not found."
    exit 1
}
