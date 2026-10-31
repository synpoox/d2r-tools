# Copies the built plugin into a game slot's global plugin folder (<game>\d2rloader\plugins). MultiSpec is a global
# plugin: it works with any mod, or none, and is never installed into a mod folder.
#   powershell -ExecutionPolicy Bypass -File install.ps1 -Game D:\d2r-slots\slot-2 [-Dll build\Release\multispec.dll]
# Refuses: a folder whose path does not contain "slot" (your own install; pass -NotASlotIKnow to override, for a
# release install you mean to make), a game or loader running from that folder, a folder without D2RLoader.
# Warns about an old mod-scoped copy (mods\*\d2rloader\plugins\multispec.dll): it would replace this one in that mod.
# The config file is written by D2RLoader itself (from the DLL) the first time; this script never replaces it.
param(
    [Parameter(Mandatory = $true)][string]$Game,
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
if (-not (Test-Path (Join-Path $Game "d2rloader"))) { throw "no d2rloader folder in $Game (install D2RLoader first)" }
if (-not (Test-Path $Dll)) { throw "no plugin at $Dll (run build.ps1)" }
$plugins = Join-Path $Game "d2rloader\plugins"
New-Item -ItemType Directory -Force -Path $plugins | Out-Null
Copy-Item $Dll (Join-Path $plugins "multispec.dll") -Force
Write-Output "installed: $(Join-Path $plugins 'multispec.dll')"
Get-ChildItem (Join-Path $Game "mods") -Directory -ErrorAction SilentlyContinue | ForEach-Object {
    $old = Join-Path $_.FullName "d2rloader\plugins\multispec.dll"
    if (Test-Path $old) { Write-Warning "old mod-scoped copy $old replaces the global one while that mod runs: delete it" }
}
Write-Output "log after the next start: $(Join-Path $Game 'd2rloader\logs')"
