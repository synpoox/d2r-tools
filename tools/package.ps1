# Packages a built Quick NPC DLL for release: quick-npc-<version>.zip plus its .sha256.
#   powershell -ExecutionPolicy Bypass -File tools\package.ps1 [-Dll plugin\build\Release\quick-npc.dll] [-Out dist]
# The version is read from plugin/CMakeLists.txt (MODKIT_PLUGIN_VERSION) and must also appear in the DLL.
param(
    [string]$Dll = "",
    [string]$Out = ""
)
$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $Dll) { $Dll = Join-Path $Root "plugin\build\Release\quick-npc.dll" }
if (-not $Out) { $Out = Join-Path $Root "dist" }
if (-not (Test-Path $Dll)) { throw "no DLL at ${Dll}: build it first (plugin\build.ps1)" }

$cmake = Get-Content (Join-Path $Root "plugin\CMakeLists.txt") -Raw
if ($cmake -notmatch 'MODKIT_PLUGIN_VERSION "([0-9]+\.[0-9]+\.[0-9]+)"') { throw "no MODKIT_PLUGIN_VERSION in plugin/CMakeLists.txt" }
$Version = $Matches[1]
if (-not (Select-String -Path $Dll -Pattern $Version -SimpleMatch -Quiet)) { throw "${Dll} does not contain version ${Version}: rebuild it" }
if (-not (Select-String -Path $Dll -Pattern 'hotkey = "LeftCtrl"' -SimpleMatch -Quiet)) { throw "${Dll} has no embedded quick-npc.toml" }

$Stage = Join-Path $Out "quick-npc-$Version"
if (Test-Path $Stage) { Remove-Item -Recurse -Force $Stage }
New-Item -ItemType Directory -Force -Path $Stage | Out-Null
Copy-Item $Dll (Join-Path $Stage "quick-npc.dll")
foreach ($f in @("README.md", "LICENSE", "THIRD_PARTY_NOTICES.txt")) { Copy-Item (Join-Path $Root $f) $Stage }
Copy-Item (Join-Path $Root "plugin\quick-npc.toml") (Join-Path $Stage "quick-npc.toml.example")
@"
Quick NPC $Version - D2RLoader plugin for Diablo II: Resurrected 3.3 (D2RLoader 1.3.1)

Install: close the game, copy quick-npc.dll to  <game>\d2rloader\plugins\
Remove any older quick-npc.dll from a mod's  mods\<Mod>\d2rloader\plugins\  folder.

Use: hold Left Ctrl and left-click a vendor, Deckard Cain, a mercenary NPC or a travel NPC.
Settings: <game>\d2rloader\config\quick-npc.toml is created on first start
(quick-npc.toml.example shows the defaults). Console: quick-npc, quick-npc reload.
"@ | Set-Content -Encoding ASCII (Join-Path $Stage "INSTALL.txt")

$Zip = Join-Path $Out "quick-npc-$Version.zip"
if (Test-Path $Zip) { Remove-Item -Force $Zip }
Compress-Archive -Path (Join-Path $Stage "*") -DestinationPath $Zip
$Hash = (Get-FileHash -Algorithm SHA256 $Zip).Hash.ToLower()
"$Hash  quick-npc-$Version.zip" | Set-Content -Encoding ASCII "$Zip.sha256"
Write-Output "packaged: $Zip ($Hash)"
