# Copy the clipboard helper to C:\WalletRemote so the firmware can launch it.
#
#   powershell -ExecutionPolicy Bypass -File tools\install-helper.ps1

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$dest = "C:\WalletRemote"

New-Item -ItemType Directory -Force -Path $dest | Out-Null
Copy-Item (Join-Path $here "clip-helper.ps1") (Join-Path $dest "clip-helper.ps1") -Force
Copy-Item (Join-Path $here "clip-helper.py")  (Join-Path $dest "clip-helper.py")  -Force

Write-Host "Installed helper to $dest" -ForegroundColor Green
Write-Host "The board types: $dest\clip-helper.ps1"
Write-Host "You do not need to start this by hand."
