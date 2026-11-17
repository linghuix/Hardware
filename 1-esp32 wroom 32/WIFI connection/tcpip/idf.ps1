# Runs idf.py for this project from a plain PowerShell window.
#   .\idf.ps1 build
#   .\idf.ps1 -p COM5 flash monitor
#   .\idf.ps1 menuconfig
# - Puts ESP-IDF's own Python 3.11 first on PATH (otherwise conda's Python is
#   picked and export.ps1 fails looking for a py3.9 environment).
# - Builds into C:\Users\<you>\esp\build\tcpip_iperf so the large build folder
#   is not synced by OneDrive.

$py = (Get-ChildItem "$env:USERPROFILE\.espressif\tools\idf-python" -Directory | Select-Object -First 1).FullName
$env:PATH = "$py;$env:PATH"
. "$env:USERPROFILE\esp\v5.5.1\esp-idf\export.ps1" | Out-Null

Push-Location $PSScriptRoot
try {
    idf.py -B "$env:USERPROFILE\esp\build\tcpip_iperf" @args
} finally {
    Pop-Location
}
