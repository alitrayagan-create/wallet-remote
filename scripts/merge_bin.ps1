# Build one flashable image: dist\wallet-remote.bin (write at 0x0).
# Includes bootloader + partitions + boot_app0 + app.
#
# Usage:  powershell -ExecutionPolicy Bypass -File scripts\merge_bin.ps1

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")
$py      = Get-WrPython
$esptool = Get-WrEsptool
$boot0   = Get-WrBootApp0
$root    = Split-Path $PSScriptRoot -Parent
$build   = Join-Path $root ".pio\build\cyd"
$dist    = Join-Path $root "dist"
New-Item -ItemType Directory -Force -Path $dist | Out-Null
$out     = Join-Path $dist "wallet-remote.bin"

foreach ($f in @("$build\bootloader.bin", "$build\partitions.bin", "$build\firmware.bin", $boot0)) {
    if (-not (Test-Path $f)) { throw "Missing: $f  (build first)" }
}

& $py $esptool --chip esp32 merge_bin -o $out `
    --flash_mode qio --flash_freq 40m --flash_size 4MB `
    0x1000  (Join-Path $build "bootloader.bin") `
    0x8000  (Join-Path $build "partitions.bin") `
    0xe000  $boot0 `
    0x10000 (Join-Path $build "firmware.bin")

Write-Host "`nSingle firmware image: $out" -ForegroundColor Green
Write-Host "Flash at 0x0 with:  powershell -File scripts\flash.ps1 COMx"
