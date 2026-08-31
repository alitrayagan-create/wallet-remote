# Flash the single merged image (wallet-remote.bin) at 0x0.
# Usage:  powershell -ExecutionPolicy Bypass -File scripts\flash.ps1 COM4
param([Parameter(Mandatory=$true)][string]$Port)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")
$py      = Get-WrPython
$esptool = Get-WrEsptool
$root    = Split-Path $PSScriptRoot -Parent
$img     = Join-Path $root "dist\wallet-remote.bin"

if (-not (Test-Path $img)) {
    throw "Missing $img - run scripts\rebuild.ps1 first"
}

Write-Host "Erasing flash..."
& $py $esptool --chip esp32 --port $Port erase_flash

Write-Host "Writing $img at 0x0..."
& $py $esptool --chip esp32 --port $Port --baud 460800 write_flash 0x0 $img
Write-Host ""
Write-Host "Done. Press RST/EN on the board if it does not reboot."
