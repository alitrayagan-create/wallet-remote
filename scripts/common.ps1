# Shared paths for Wallet Remote scripts (no machine-specific usernames).

function Get-WrPython {
    $candidates = @(
        $env:WALLET_REMOTE_PYTHON
        "$env:LOCALAPPDATA\Python\bin\python.exe"
        "$env:LOCALAPPDATA\Programs\Python\Python313\python.exe"
        "$env:LOCALAPPDATA\Programs\Python\Python312\python.exe"
        "$env:LOCALAPPDATA\Programs\Python\Python311\python.exe"
    )
    foreach ($c in $candidates) {
        if ($c -and (Test-Path $c)) { return $c }
    }
    foreach ($name in @("python", "py")) {
        $cmd = Get-Command $name -ErrorAction SilentlyContinue
        if ($cmd) { return $cmd.Source }
    }
    throw "Python 3 not found. Install Python and add it to PATH, or set WALLET_REMOTE_PYTHON."
}

function Get-WrEsptool {
    $p = Join-Path $env:USERPROFILE ".platformio\packages\tool-esptoolpy\esptool.py"
    if (Test-Path $p) { return $p }
    throw "esptool not found at $p - install PlatformIO and build once."
}

function Get-WrBootApp0 {
    $p = Join-Path $env:USERPROFILE ".platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin"
    if (Test-Path $p) { return $p }
    throw "boot_app0.bin not found at $p - install PlatformIO and build once."
}
