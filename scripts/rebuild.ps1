# Rebuild the firmware and merge dist\wallet-remote.bin (flash at 0x0).
#
# Usage:  powershell -ExecutionPolicy Bypass -File scripts\rebuild.ps1

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")
$py   = Get-WrPython
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root

Write-Host "=== Building... ===" -ForegroundColor Cyan
& $py -m platformio run -e cyd
if ($LASTEXITCODE -ne 0) { Write-Host "BUILD FAILED - fix errors above, nothing was flashed." -ForegroundColor Red; exit 1 }

$build = Join-Path $root ".pio\build\cyd"
$dist  = Join-Path $root "dist"
New-Item -ItemType Directory -Force -Path $dist | Out-Null
Copy-Item "$build\firmware.bin"   "$dist\firmware.bin"   -Force
Copy-Item "$build\bootloader.bin" "$dist\bootloader.bin" -Force
Copy-Item "$build\partitions.bin" "$dist\partitions.bin" -Force

Write-Host "=== Merging single flash image ===" -ForegroundColor Cyan
& (Join-Path $PSScriptRoot "merge_bin.ps1")
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "`n=== FRESH BUILD READY ===" -ForegroundColor Green
Get-Item "$dist\wallet-remote.bin" | Select-Object FullName,Length,LastWriteTime | Format-List
Write-Host "Flash the ONE file dist\wallet-remote.bin at 0x0:" -ForegroundColor Yellow
Write-Host "  powershell -ExecutionPolicy Bypass -File scripts\flash.ps1 COM5" -ForegroundColor Yellow
