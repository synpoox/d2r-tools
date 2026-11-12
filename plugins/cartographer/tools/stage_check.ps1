<#
.SYNOPSIS
    Stages a Cartographer game check in one test slot: lock, test mod, DLL, tables, test saves, installer, settings.

.DESCRIPTION
    Developer tool for the game checks in docs/cartographer/testing.md. It only ever works in a test slot (2 or higher,
    never slot 1, the owner's own game) and never writes to a real save folder.

    First run for a slot: takes the slot lock (d2rAI-toolkit game_slots.py lock, never --force), creates the slot's test
    mod from a template mod (its excel folder and a modinfo.json with the slot's own savepath), copies the DLL, the
    Cartographer tables and the test saves, runs install.ps1 on the test mod's excel folder, and optionally writes
    AutoCheck=1 / AutoCheckMap=<name> into the slot's cartographer.txt.
    Later runs: the same steps, but a file is copied only when its SHA-256 differs from the one already in the slot,
    so a repeat check re-stages just what changed (the DLL, the tables, the saves).

    -Restore runs the toolkit's release_install.py restore (a slot with no release install has nothing to restore; that
    is reported, not fatal) and unlocks the slot. The staged files stay in the slot; the next run compares them by hash.

    -WhatIf prints every action and changes nothing: no lock, no copy, no installer run.
    The game is never launched; the launch command and the log path are printed.

.PARAMETER Slot
    Test slot number, 2 or higher. Slot 1 is refused.
.PARAMETER Owner
    Lock owner and run id (MODKIT_RUN), for example the name of your lane.
.PARAMETER Dll
    The built cartographer.dll to place in the slot's global plugins folder.
.PARAMETER Tables
    Folder with maps.txt, cartographer.txt, bosses.txt (any *.txt) copied to excel\d2rloader\cartographer.
.PARAMETER Saves
    Folder with test saves (.d2s and the files next to them) copied to the slot's own save folder.
.PARAMETER AutoCheckMap
    Writes AutoCheck=1 and AutoCheckMap=<name> into the slot's cartographer.txt (settings of PR #176; unknown settings
    are harmless to a build without them).
.PARAMETER ModSource
    Template test mod folder used the first time a slot has no test mod (default: the Slot11 test mod; read only).
.PARAMETER Tileset
    Cave (default) or Crypt, passed to install.ps1.
.PARAMETER Project
    Project name for the slot table (default: MODKIT_PROJECT, else none).
.PARAMETER Toolkit
    d2rAI-toolkit checkout (default: the d2rAI-toolkit folder next to this repo's folder).
.PARAMETER Restore
    Restore the slot and unlock it.
.PARAMETER WhatIf
    Print the actions without doing them.

.EXAMPLE
    plugins\cartographer\tools\stage_check.ps1 -Slot 11 -Owner carto -Dll build\cartographer.dll -Tables C:\scratch\tables -Saves C:\scratch\saves -AutoCheckMap HellCave
    plugins\cartographer\tools\stage_check.ps1 -Slot 11 -Owner carto -Restore
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][int]$Slot,
    [Parameter(Mandatory)][string]$Owner,
    [string]$Dll,
    [string]$Tables,
    [string]$Saves,
    [string]$AutoCheckMap,
    [string]$ModSource,
    [ValidateSet('Cave', 'Crypt')][string]$Tileset = 'Cave',
    [string]$Project = $env:MODKIT_PROJECT,
    [string]$Toolkit,
    [switch]$Restore,
    [switch]$WhatIf
)

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$installer = Join-Path $here 'install.ps1'
$latin1 = [System.Text.Encoding]::GetEncoding(28591)

function Say([string]$text) { Write-Host $text }
function Plan([string]$text) { if ($WhatIf) { Write-Host "[WhatIf] $text" } else { Write-Host $text } }

if ($Slot -lt 2) { throw "Slot $Slot is refused: slot 1 is the owner's own game. Use a test slot (2 or higher)." }
if ($Owner -notmatch '^[A-Za-z0-9._-]+$') { throw "Owner '$Owner' must be letters, digits, '.', '_' or '-' (it names the lock and the run)." }

if (-not $Toolkit) {
    $repoRoot = Split-Path (Split-Path (Split-Path $here -Parent) -Parent) -Parent  # plugins\cartographer\tools -> repo
    $Toolkit = Join-Path (Split-Path $repoRoot -Parent) 'd2rAI-toolkit'
    if (-not (Test-Path $Toolkit)) { $Toolkit = Join-Path $env:USERPROFILE 'source\repos\d2rAI-toolkit' }
}
$testing = Join-Path $Toolkit 'testing'
if (-not (Test-Path (Join-Path $testing 'game_slots.py'))) { throw "No d2rAI-toolkit testing folder at $testing. Pass -Toolkit." }
if (-not $env:MODKIT_TESTING_CONFIG) { $env:MODKIT_TESTING_CONFIG = 'C:\D2RTestSlots\modkit-testing.toml' }
if (-not (Test-Path $env:MODKIT_TESTING_CONFIG)) { throw "No testing config at $env:MODKIT_TESTING_CONFIG (set MODKIT_TESTING_CONFIG)." }
$env:MODKIT_RUN = $Owner
$env:MODKIT_SLOT = "$Slot"
if ($Project) { $env:MODKIT_PROJECT = $Project }

# Paths come from the toolkit's own config code, so nothing here guesses a folder (single quotes: Windows PowerShell 5.1
# drops double quotes from a native command's arguments).
$py = @'
import json, os, sys
sys.path.insert(0, sys.argv[1])
import testing_config as tc
cfg = tc.load()
n = int(sys.argv[2])
cfg.check_slot_number(n)
print(json.dumps({
    'slotDir': cfg.slot_dir(n), 'installDir': cfg.install_dir, 'modDir': cfg.mod_dir(n),
    'plugins': os.path.normpath(cfg.plugins_dir(n)), 'saveDir': cfg.save_dir(n), 'slot1SaveDir': cfg.save_dir(1),
    'saveName': cfg.save_name(n), 'modFolder': cfg.mod_folder(),
}))
'@
$raw = & python -I -c $py $testing $Slot
if ($LASTEXITCODE -ne 0) { throw "The toolkit config could not be read for slot $Slot." }
$p = $raw | ConvertFrom-Json
$norm = { param($x) [System.IO.Path]::GetFullPath($x).TrimEnd('\').ToLowerInvariant() }
if ((& $norm $p.slotDir) -eq (& $norm $p.installDir)) { throw "Slot $Slot resolves to the owner's own install. Refused." }
if ((& $norm $p.saveDir) -eq (& $norm $p.slot1SaveDir)) { throw "Slot $Slot would use the owner's real save folder. Refused." }

$modName = $p.modFolder
$excel = Join-Path $p.modDir "$modName.mpq\data\global\excel"
$cartoTables = Join-Path $excel 'd2rloader\cartographer'
$cartoLog = Join-Path $p.slotDir 'd2rloader\logs\cartographer.log'

function Invoke-Toolkit([string]$script, [string[]]$tkArgs) {
    & python -I (Join-Path $testing $script) @tkArgs
    return $LASTEXITCODE
}

if ($Restore) {
    Say "Restoring slot $Slot (owner $Owner)."
    if ($WhatIf) {
        Plan "python release_install.py restore --slot $Slot   (when the slot holds a release install)"
        Plan "python game_slots.py unlock $Slot $Owner"
        return
    }
    $rc = Invoke-Toolkit 'release_install.py' @('restore', '--slot', "$Slot")
    if ($rc -ne 0) { Say "release_install restore exited $rc (a slot with no release install has nothing to restore)." }
    $rc = Invoke-Toolkit 'game_slots.py' @('unlock', "$Slot", $Owner)
    if ($rc -ne 0) { throw "Could not unlock slot $Slot (exit $rc). It is not forced; check 'game_slots.py status'." }
    Say "Slot $Slot is unlocked."
    return
}

if (-not $Dll) { throw 'Pass -Dll <path to cartographer.dll>.' }
if (-not (Test-Path -PathType Leaf $Dll)) { throw "No DLL at $Dll." }
if ($Tables -and -not (Test-Path -PathType Container $Tables)) { throw "No tables folder at $Tables." }
if ($Saves) {
    if (-not (Test-Path -PathType Container $Saves)) { throw "No saves folder at $Saves." }
    if ((& $norm $Saves) -eq (& $norm $p.slot1SaveDir)) { throw "-Saves points at the owner's real save folder. Use a folder of test saves." }
}

# 1. Lock (never --force).
Say "Slot $Slot, owner $Owner, test mod $modName, slot folder $($p.slotDir)"
if ($WhatIf) {
    Plan "python game_slots.py lock $Slot $Owner$(if ($Project) { " --project $Project" })   (refuses slot 1 and a slot held by someone else)"
} else {
    $lockArgs = @('lock', "$Slot", $Owner)
    if ($Project) { $lockArgs += @('--project', $Project) }
    $rc = Invoke-Toolkit 'game_slots.py' $lockArgs
    if ($rc -ne 0) { throw "Could not lock slot $Slot (exit $rc). Not forced: wait for it or pick another slot." }
}

# 2. Copy a file only when its SHA-256 differs from the one in the slot.
function Copy-IfChanged([string]$from, [string]$to, [byte[]]$bytes) {
    $new = if ($null -ne $bytes) { $bytes } else { [System.IO.File]::ReadAllBytes($from) }
    $same = $false
    if (Test-Path -PathType Leaf $to) {
        $sha = [System.Security.Cryptography.SHA256]::Create()
        $same = ([BitConverter]::ToString($sha.ComputeHash($new)) -eq [BitConverter]::ToString($sha.ComputeHash([System.IO.File]::ReadAllBytes($to))))
    }
    $leaf = Split-Path $to -Leaf
    if ($same) { Say "  unchanged: $leaf"; return }
    Plan "  copy: $leaf -> $to"
    if (-not $WhatIf) {
        $null = New-Item -ItemType Directory -Force (Split-Path $to -Parent)
        [System.IO.File]::WriteAllBytes($to, $new)
    }
}

# 3. Stage the test mod once per slot.
if (-not (Test-Path $excel)) {
    if (-not $ModSource) { $ModSource = Join-Path (Split-Path $p.slotDir -Parent) "Slot11\mods\$modName" }
    $srcExcel = Join-Path $ModSource "$modName.mpq\data\global\excel"
    if (-not (Test-Path $srcExcel)) { throw "Slot $Slot has no test mod and the template $ModSource has no excel folder. Pass -ModSource." }
    Plan "Staging the test mod once: copy the files of $srcExcel to $excel (not the d2rloader tables)"
    if (-not $WhatIf) {
        $null = New-Item -ItemType Directory -Force $excel
        Get-ChildItem $srcExcel -File | Copy-Item -Destination $excel
        $info = "{`n  `"name`": `"$modName`",`n  `"savepath`": `"$($p.saveName)/`"`n}`n"
        [System.IO.File]::WriteAllText((Join-Path $p.modDir "$modName.mpq\modinfo.json"), $info)
        [System.IO.File]::WriteAllText((Join-Path $p.modDir 'modinfo.json'), $info)
    }
} else {
    Say "Test mod already staged in slot $Slot."
}

# 4. The DLL, the tables, the saves: only what changed.
Say 'DLL:'
Copy-IfChanged $Dll (Join-Path $p.plugins (Split-Path $Dll -Leaf)) $null

if ($Tables) {
    Say 'Tables:'
    foreach ($f in Get-ChildItem $Tables -File -Filter '*.txt') {
        if ($f.Name -ieq 'cartographer.txt' -and $AutoCheckMap) { continue }  # written with the settings below
        Copy-IfChanged $f.FullName (Join-Path $cartoTables $f.Name) $null
    }
}

if ($Saves) {
    Say "Saves -> $($p.saveDir):"
    foreach ($f in Get-ChildItem $Saves -File) { Copy-IfChanged $f.FullName (Join-Path $p.saveDir $f.Name) $null }
}

# 5. install.ps1 on the test mod's excel folder (safe to run twice).
if ($WhatIf) {
    Plan "powershell install.ps1 -ExcelDir `"$excel`" -Tileset $Tileset"
} else {
    & $installer -ExcelDir $excel -Tileset $Tileset
}

# 6. AutoCheck settings: the source cartographer.txt plus the two rows, compared by hash like every other file.
if ($AutoCheckMap) {
    Say "AutoCheck settings (AutoCheck=1, AutoCheckMap=$AutoCheckMap):"
    $dest = Join-Path $cartoTables 'cartographer.txt'
    $srcFile = if ($Tables -and (Test-Path (Join-Path $Tables 'cartographer.txt'))) { Join-Path $Tables 'cartographer.txt' } else { $dest }
    $text = if (Test-Path $srcFile) { $latin1.GetString([System.IO.File]::ReadAllBytes($srcFile)) } else { "Setting`tValue`r`n" }
    $nl = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
    $lines = [System.Collections.Generic.List[string]]($text -split "\r?\n")
    while ($lines.Count -gt 0 -and $lines[$lines.Count - 1] -eq '') { $lines.RemoveAt($lines.Count - 1) }
    foreach ($kv in @(@('AutoCheck', '1'), @('AutoCheckMap', $AutoCheckMap))) {
        $row = "$($kv[0])`t$($kv[1])"
        $at = -1
        for ($i = 1; $i -lt $lines.Count; $i++) { if ($lines[$i].Split("`t")[0].Trim() -ieq $kv[0]) { $at = $i; break } }
        if ($at -ge 0) { $lines[$at] = $row } else { $lines.Add($row) }
    }
    Copy-IfChanged $srcFile $dest ($latin1.GetBytes(($lines -join $nl) + $nl))
}

# 7. How to run it.
Say ''
Say 'Staged. Launch (the game is not started by this script):'
Say "  `$env:MODKIT_TESTING_CONFIG='$($env:MODKIT_TESTING_CONFIG)'; `$env:MODKIT_RUN='$Owner'; python `"$testing\game_ctl.py`" --slot $Slot start"
Say "Plugin log: $cartoLog"
Say "Loader log: $($p.slotDir)\d2rloader\logs\d2rloader.log"
Say "When done: stage_check.ps1 -Slot $Slot -Owner $Owner -Restore"
