# Flash bootloader + partitions + boot_app0 + app at the correct offsets.
# Prefer scripts\flash.ps1 (single merged image) unless you need this split write.
#
# Usage:  powershell -ExecutionPolicy Bypass -File scripts\flash_all.ps1 COM5
param([Parameter(Mandatory=$true)][string]$Port)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")
$py      = Get-WrPython
$esptool = Get-WrEsptool
$boot0   = Get-WrBootApp0
$root    = Split-Path $PSScriptRoot -Parent
$build   = Join-Path $root ".pio\build\cyd"

foreach ($f in @("$build\bootloader.bin","$build\partitions.bin","$build\firmware.bin",$boot0)) {
    if (-not (Test-Path $f)) { throw "Missing: $f  (build first: python -m platformio run -e cyd)" }
}

Write-Host "Erasing flash..."
& $py $esptool --chip esp32 --port $Port erase_flash

Write-Host "Writing all sections..."
& $py $esptool --chip esp32 --port $Port --baud 460800 write_flash -z `
    0x1000  "$build\bootloader.bin" `
    0x8000  "$build\partitions.bin" `
    0xe000  $boot0 `
    0x10000 "$build\firmware.bin"

Write-Host "`nDone. Press the board's RST/EN button (or unplug/replug) to boot."
