<#
.SYNOPSIS
    Stages a Cartographer game check in one test slot: lock, test mod, DLL, tables, test saves, installer, settings.

.DESCRIPTION
    Developer tool for the game checks in docs/cartographer/testing.md. It only ever works in the slots the owner approved
    for Cartographer checks (5, 6, 7 and 11), never slot 1 (the owner's own game) and never another project's slot, and it
    never writes to a real save folder. Use the same -Owner only for one lane at a time: the lock is "already ours" for a
    second run with the same owner, so two lanes with one owner can stage over each other.

    First run for a slot: takes the slot lock (d2rAI-toolkit game_slots.py lock, never --force), creates the slot's test
    mod (a modinfo.json with the slot's own savepath, and an excel folder seeded with copies of the vanilla tables
    install.ps1 needs: levels, lvlmaze, superuniques, monstats, monumod; only files the slot lacks), copies the DLL, the
    Cartographer tables and the test saves, runs install.ps1 on the test mod's excel folder, and optionally writes
    AutoCheck=1 / AutoCheckMap=<name> into the slot's cartographer.txt.
    Later runs: the same steps, but a file is copied only when its SHA-256 differs from the one already in the slot,
    so a repeat check re-stages just what changed (the DLL, the tables, the saves).

    All checks (inputs, install.ps1, vanilla tables, slot, save and install-folder guards, no game running in the slot)
    run before the lock. If a step fails after the lock, the error is printed with the exact -Restore command; the slot
    stays locked.

    -Restore runs the toolkit's release_install.py restore when the slot holds a release install (otherwise there is
    nothing to restore and that is reported), removes the cartographer DLL this script placed and the AutoCheck and
    AutoCheckMap rows it wrote, and unlocks the slot. Other staged files stay; the next run compares them by hash.

    -WhatIf prints every action and changes nothing: no lock, no copy, no installer run.
    The game is never launched; the launch command and the log path are printed.

.PARAMETER Slot
    Slot number: 5, 6, 7 or 11 (the slots approved for Cartographer checks). Any other slot is refused.
.PARAMETER Owner
    Lock owner and run id (MODKIT_RUN), for example the name of your lane.
.PARAMETER Dll
    The built cartographer.dll to place in the slot's global plugins folder (with -Restore, optional: names the DLL to remove).
.PARAMETER Tables
    Folder with maps.txt, cartographer.txt, bosses.txt (any *.txt) copied to excel\d2rloader\cartographer.
.PARAMETER Saves
    Folder with test saves (.d2s and the files next to them) copied to the slot's own save folder. Refused when it is
    inside the owner's real save folders (the vanilla Saved Games root and every mod folder under it), junctions resolved.
.PARAMETER AutoCheckMap
    Writes AutoCheck=1 and AutoCheckMap=<name> into the slot's cartographer.txt (settings of PR #176; unknown settings
    are harmless to a build without them).
.PARAMETER VanillaExcel
    Folder with the vanilla tables the test mod's excel is seeded from (copy only, never edited). Default: the d2r-game
    d2rloader compiler data next to this repo.
.PARAMETER Tileset
    Cave (default) or Crypt, passed to install.ps1.
.PARAMETER Project
    Project name for the slot table (default: MODKIT_PROJECT, else none).
.PARAMETER Toolkit
    d2rAI-toolkit checkout (default: the d2rAI-toolkit folder next to this repo's folder).
.PARAMETER Restore
    Restore the slot (remove the DLL and AutoCheck rows this script placed) and unlock it.
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
    [string]$VanillaExcel,
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

# Slots the owner approved for Cartographer checks. The config says D2RToolsTest owns Slot11; 5, 6 and 7 are approved too.
$AllowedSlots = @(5, 6, 7, 11)
# Tables install.ps1 needs in the test mod's excel folder (the first two are required, the rest only matter for bosses).
$SeedTables = @('levels.txt', 'lvlmaze.txt', 'superuniques.txt', 'monstats.txt', 'monumod.txt')

function Say([string]$text) { Write-Host $text }
function Plan([string]$text) { if ($WhatIf) { Write-Host "[WhatIf] $text" } else { Write-Host $text } }

if ($AllowedSlots -notcontains $Slot) {
    throw "Slot $Slot is refused: Cartographer checks may only use slots $($AllowedSlots -join ', '). Slot 1 is the owner's own game; the other slots belong to other projects."
}
if ($Owner -notmatch '^[A-Za-z0-9._-]+$') { throw "Owner '$Owner' must be letters, digits, '.', '_' or '-' (it names the lock and the run)." }

if (-not $Toolkit) {
    $repoRoot = Split-Path (Split-Path (Split-Path $here -Parent) -Parent) -Parent  # plugins\cartographer\tools -> repo
    $Toolkit = Join-Path (Split-Path $repoRoot -Parent) 'd2rAI-toolkit'
    if (-not (Test-Path $Toolkit)) { $Toolkit = Join-Path $env:USERPROFILE 'source\repos\d2rAI-toolkit' }
}
if (-not $VanillaExcel) { $VanillaExcel = Join-Path $env:USERPROFILE 'source\repos\d2r-game\d2rloader\data\compiler\3.3.0\data\global\excel' }
$testing = Join-Path $Toolkit 'testing'
if (-not (Test-Path (Join-Path $testing 'game_slots.py'))) { throw "No d2rAI-toolkit testing folder at $testing. Pass -Toolkit." }
if (-not $env:MODKIT_TESTING_CONFIG) { $env:MODKIT_TESTING_CONFIG = 'C:\D2RTestSlots\modkit-testing.toml' }
if (-not (Test-Path $env:MODKIT_TESTING_CONFIG)) { throw "No testing config at $env:MODKIT_TESTING_CONFIG (set MODKIT_TESTING_CONFIG)." }
$env:MODKIT_RUN = $Owner
$env:MODKIT_SLOT = "$Slot"
if ($Project) { $env:MODKIT_PROJECT = $Project }

# Paths come from the toolkit's own config code, so nothing here guesses a folder. Every path is returned as a real path
# (junctions and symlinks resolved). Single quotes: Windows PowerShell 5.1 drops double quotes from a native command's arguments.
$py = @'
import json, os, sys
sys.path.insert(0, sys.argv[1])
import testing_config as tc
import game_slots
cfg = tc.load()
n = int(sys.argv[2])
cfg.check_slot_number(n)
real = lambda x: os.path.normcase(os.path.realpath(x))
saves_root = cfg.path(cfg['saves']['root'])  # <Saved Games>\Diablo II Resurrected\mods
print(json.dumps({
    'slotDir': real(cfg.slot_dir(n)), 'installDir': real(cfg.install_dir), 'modDir': real(cfg.mod_dir(n)),
    'plugins': real(cfg.plugins_dir(n)), 'saveDir': real(cfg.save_dir(n)),
    'realSaveRoots': [real(os.path.dirname(saves_root)), real(saves_root), real(cfg.save_dir(1))],
    'ownerSaveDir': real(cfg.save_dir(1)),
    'saveName': cfg.save_name(n), 'modFolder': cfg.mod_folder(),
    'running': [list(x) for x in game_slots.slot_processes(cfg, n)],
}))
'@
$raw = & python -I -c $py $testing $Slot
if ($LASTEXITCODE -ne 0) { throw "The toolkit config could not be read for slot $Slot." }
$p = $raw | ConvertFrom-Json

# Real path of any path, through the same Python the toolkit uses (resolves junctions, symlinks, 8.3 names, case).
function Get-Real([string]$path) {
    $r = & python -I -c 'import os,sys;print(os.path.normcase(os.path.realpath(sys.argv[1])))' $path
    if ($LASTEXITCODE -ne 0 -or -not $r) { throw "Could not resolve the real path of $path." }
    return ([string]$r).TrimEnd('\')
}
function Test-Under([string]$child, [string]$parent) {
    $c = $child.TrimEnd('\'); $q = $parent.TrimEnd('\')
    return ($c -eq $q) -or $c.StartsWith($q + '\')
}

$modName = $p.modFolder
$modDir = $p.modDir
$excel = Join-Path $modDir "$modName.mpq\data\global\excel"
$cartoTables = Join-Path $excel 'd2rloader\cartographer'
$slotDir = $p.slotDir
$cartoLog = Join-Path $slotDir 'd2rloader\logs\cartographer.log'
$rerun = "plugins\cartographer\tools\stage_check.ps1 -Slot $Slot -Owner $Owner -Restore"

# Guards (read only; all before the lock). Every place this script writes must be outside the owner's own install,
# and the slot's save folder must not be the owner's own.
foreach ($t in @($slotDir, $modDir, $p.plugins, $p.saveDir, $excel)) {
    if (Test-Under (Get-Real $t) $p.installDir) { throw "Slot $Slot target $t is inside the owner's own install ($($p.installDir)). Refused." }
}
if (Test-Under $p.saveDir $p.ownerSaveDir) { throw "Slot $Slot would use the owner's real save folder ($($p.ownerSaveDir)). Refused." }
if ($p.running.Count -gt 0) { throw "A game is running in slot $Slot (process $(($p.running | ForEach-Object { $_[0] }) -join ', ')). Close it first." }

function Invoke-Toolkit([string]$script, [string[]]$tkArgs) {
    & python -I (Join-Path $testing $script) @tkArgs
    return $LASTEXITCODE
}

if ($Restore) {
    Say "Restoring slot $Slot (owner $Owner)."
    $dllName = if ($Dll) { Split-Path $Dll -Leaf } else { 'cartographer.dll' }
    $dllPath = Join-Path $p.plugins $dllName
    $settingsFile = Join-Path $cartoTables 'cartographer.txt'
    if ($WhatIf) {
        Plan "python release_install.py status --slot $Slot, then restore when the slot holds a release install"
        Plan "remove $dllPath (when present)"
        Plan "remove the AutoCheck and AutoCheckMap rows from $settingsFile (when present)"
        Plan "python game_slots.py unlock $Slot $Owner"
        return
    }
    $status = (& python -I (Join-Path $testing 'release_install.py') status --slot "$Slot" | Out-String)
    if ($LASTEXITCODE -ne 0) { throw "release_install status failed for slot $Slot (exit $LASTEXITCODE): $status" }
    if ($status -match 'no release install') {
        Say "Slot $Slot holds no release install: nothing to restore from one."
    } else {
        $rc = Invoke-Toolkit 'release_install.py' @('restore', '--slot', "$Slot")
        if ($rc -ne 0) { throw "release_install restore failed for slot $Slot (exit $rc). The slot is still locked." }
    }
    if (Test-Path -PathType Leaf $dllPath) { Remove-Item -LiteralPath $dllPath -Force; Say "Removed $dllPath" }
    if (Test-Path -PathType Leaf $settingsFile) {
        $text = $latin1.GetString([System.IO.File]::ReadAllBytes($settingsFile))
        $nl = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
        $lines = @($text -split "\r?\n")
        $kept = @(for ($i = 0; $i -lt $lines.Count; $i++) {
            if ($i -gt 0 -and @('autocheck', 'autocheckmap') -contains $lines[$i].Split("`t")[0].Trim().ToLowerInvariant()) { continue }
            $lines[$i]
        })
        if ($kept.Count -ne $lines.Count) {
            [System.IO.File]::WriteAllBytes($settingsFile, $latin1.GetBytes($kept -join $nl))
            Say "Removed the AutoCheck rows from $settingsFile"
        }
    }
    $rc = Invoke-Toolkit 'game_slots.py' @('unlock', "$Slot", $Owner)
    if ($rc -ne 0) { throw "Could not unlock slot $Slot (exit $rc). It is not forced; check 'game_slots.py status'." }
    Say "Slot $Slot is unlocked."
    return
}

# Preflight: every input and prerequisite is checked here, before the lock is taken.
if (-not $Dll) { throw 'Pass -Dll <path to cartographer.dll>.' }
if (-not (Test-Path -PathType Leaf $Dll)) { throw "No DLL at $Dll." }
if (-not (Test-Path -PathType Leaf $installer)) { throw "No install.ps1 at $installer." }
if ($Tables -and -not (Test-Path -PathType Container $Tables)) { throw "No tables folder at $Tables." }
if ($Saves) {
    if (-not (Test-Path -PathType Container $Saves)) { throw "No saves folder at $Saves." }
    $realSaves = Get-Real $Saves
    foreach ($r in $p.realSaveRoots) {
        if (Test-Under $realSaves $r) { throw "-Saves ($Saves, real path $realSaves) is inside the owner's real save folders ($r). Use a scratch folder of test saves." }
    }
    if ((Test-Under $realSaves $p.slotDir) -or (Test-Under $realSaves $p.installDir)) { throw "-Saves ($Saves) is inside a game folder. Use a scratch folder of test saves." }
}
if (-not (Test-Path -PathType Container $VanillaExcel)) { throw "No vanilla tables folder at $VanillaExcel. Pass -VanillaExcel." }
$missingSeed = @()
foreach ($name in @('levels.txt', 'lvlmaze.txt')) {
    if (-not (Test-Path -PathType Leaf (Join-Path $excel $name)) -and -not (Test-Path -PathType Leaf (Join-Path $VanillaExcel $name))) { $missingSeed += $name }
}
if ($missingSeed.Count -gt 0) { throw "Neither the slot's test mod nor $VanillaExcel has $($missingSeed -join ', '), which install.ps1 requires." }

# 1. Lock (never --force).
Say "Slot $Slot, owner $Owner, test mod $modName, slot folder $slotDir"
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

try {
    # 3. Stage the test mod: modinfo with the slot's own savepath, and the vanilla tables install.ps1 needs (copies; only files the slot lacks).
    $info = "{`n  `"name`": `"$modName`",`n  `"savepath`": `"$($p.saveName)/`"`n}`n"
    foreach ($mi in @((Join-Path $modDir "$modName.mpq\modinfo.json"), (Join-Path $modDir 'modinfo.json'))) {
        if (-not (Test-Path -PathType Leaf $mi)) {
            Plan "Test mod: write $mi (savepath $($p.saveName)/)"
            if (-not $WhatIf) { $null = New-Item -ItemType Directory -Force (Split-Path $mi -Parent); [System.IO.File]::WriteAllText($mi, $info) }
        }
    }
    foreach ($name in $SeedTables) {
        $dest = Join-Path $excel $name
        $src = Join-Path $VanillaExcel $name
        if ((Test-Path -PathType Leaf $dest) -or -not (Test-Path -PathType Leaf $src)) { continue }
        Plan "Test mod: seed $name from the vanilla tables (copy; the original is not touched)"
        if (-not $WhatIf) { $null = New-Item -ItemType Directory -Force $excel; Copy-Item -LiteralPath $src -Destination $dest }
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
} catch {
    Write-Host "Staging failed: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Slot $Slot is STILL LOCKED by $Owner. Fix the cause and run this again, or release it with:" -ForegroundColor Red
    Write-Host "  $rerun" -ForegroundColor Red
    throw
}

# 7. How to run it.
Say ''
Say 'Staged. Launch (the game is not started by this script):'
Say "  `$env:MODKIT_TESTING_CONFIG='$($env:MODKIT_TESTING_CONFIG)'; `$env:MODKIT_RUN='$Owner'; python `"$testing\game_ctl.py`" --slot $Slot start"
Say "Plugin log: $cartoLog"
Say "Loader log: $slotDir\d2rloader\logs\d2rloader.log"
Say "When done: stage_check.ps1 -Slot $Slot -Owner $Owner -Restore"
