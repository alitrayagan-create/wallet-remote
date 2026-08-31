# Wallet Remote — Bluetooth clipboard helper
# The board can launch this itself (Win+R → PowerShell). You can still run it
# by hand if you want it to stay open:
#
#   powershell -ExecutionPolicy Bypass -File tools\clip-helper.ps1

param([int]$QuitAfter = 0)

$ErrorActionPreference = "Stop"
try { $Host.UI.RawUI.WindowTitle = "WalletRemoteHelper" } catch {}

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$script = Join-Path $here "clip-helper.py"

$candidates = @(
    $env:WALLET_REMOTE_PYTHON
    "$env:LOCALAPPDATA\Python\bin\python.exe"
    "$env:LOCALAPPDATA\Programs\Python\Python313\python.exe"
    "$env:LOCALAPPDATA\Programs\Python\Python312\python.exe"
    "$env:LOCALAPPDATA\Programs\Python\Python311\python.exe"
)

$py = $null
foreach ($c in $candidates) {
    if ($c -and (Test-Path $c)) { $py = $c; break }
}
if (-not $py) {
    $cmd = Get-Command python -ErrorAction SilentlyContinue
    if ($cmd) { $py = $cmd.Source }
}
if (-not $py) {
    $cmd = Get-Command py -ErrorAction SilentlyContinue
    if ($cmd) { $py = $cmd.Source }
}
if (-not $py) {
    throw "Python not found. Install Python 3, then run this script again."
}

$pyArgs = @($script)
if ($QuitAfter -gt 0) {
    $pyArgs += "--quit-after"
    $pyArgs += "$QuitAfter"
}
$pyArgs += $args

& $py @pyArgs
exit $LASTEXITCODE
