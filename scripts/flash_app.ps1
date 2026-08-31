# Fast app-only flash: writes dist\firmware.bin at 0x10000.
# Prefer scripts\flash.ps1 for a full image.
#
# Usage:  powershell -ExecutionPolicy Bypass -File scripts\flash_app.ps1 [COM5]
param([string]$Port = "")

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")
$py      = Get-WrPython
$esptool = Get-WrEsptool
$root    = Split-Path $PSScriptRoot -Parent
$app     = Join-Path $root "dist\firmware.bin"

if (-not (Test-Path $app)) { throw "Missing $app - run scripts\rebuild.ps1 first" }
Write-Host ("Flashing: {0}" -f (Get-Item $app).LastWriteTime) -ForegroundColor Cyan

$portArgs = @()
if ($Port -ne "") { $portArgs = @("--port", $Port) }

& $py $esptool --chip esp32 @portArgs --baud 460800 write_flash -z 0x10000 $app
Write-Host "`nDone. Press RST on the board." -ForegroundColor Green
