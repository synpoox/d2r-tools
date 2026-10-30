# Copies the built plugin into a game slot's mod folder.
#   powershell -ExecutionPolicy Bypass -File install.ps1 -Game D:\d2r-slots\slot-2 -Mod MyMod [-Dll build\Release\multispec.dll]
# Refuses: a folder whose path does not contain "slot" (your own install; pass -NotASlotIKnow to override, for a
# release install you mean to make), a game or loader running from that folder, a missing mod folder.
# The config file is written by D2RLoader itself (from the DLL) the first time; this script never replaces it.
param(
    [Parameter(Mandatory = $true)][string]$Game,
    [Parameter(Mandatory = $true)][string]$Mod,
    [string]$Dll = "",
    [switch]$NotASlotIKnow
)
$ErrorActionPreference = 'Stop'
if (-not $Dll) { $Dll = Join-Path $PSScriptRoot "build\Release\multispec.dll" }
$Game = (Resolve-Path $Game).Path
if (-not $NotASlotIKnow -and $Game -notmatch 'slot') {
    throw "$Game does not look like a test slot. Install into a toolkit game slot (testing/game_slots.py), not your own game."
}
$running = Get-Process -ErrorAction SilentlyContinue | Where-Object {
    $_.Path -and $_.Path.StartsWith($Game, [StringComparison]::OrdinalIgnoreCase) }
if ($running) { throw "close the game in $Game first (running: $($running.ProcessName -join ', '))" }
$modDir = Join-Path $Game "mods\$Mod"
if (-not (Test-Path $modDir)) { throw "no mod folder $modDir" }
if (-not (Test-Path $Dll)) { throw "no plugin at $Dll (run build.ps1)" }
$plugins = Join-Path $modDir "d2rloader\plugins"
New-Item -ItemType Directory -Force -Path $plugins | Out-Null
Copy-Item $Dll (Join-Path $plugins "multispec.dll") -Force
Write-Output "installed: $(Join-Path $plugins 'multispec.dll')"
Write-Output "log after the next start: $(Join-Path $modDir 'd2rloader\logs')"
