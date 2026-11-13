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
    install.ps1 needs: levels, lvlmaze, superuniques, monstats, monumod, and itemstatcost for the map stats; plus a
    d2rloader\core-ui\automap.txt with one row, which the game needs at startup; only files the slot lacks, but an
    automap.txt this script placed earlier is replaced when it has no data row or -CoreUiAutomap is given), points
    default_mod in the slot's d2rloader\config\d2rloader.toml at the test mod (-Restore sets the old value back), copies the DLL, the
    Cartographer tables and the test saves, runs install.ps1 on the test mod's excel folder, and optionally writes
    AutoCheck=1 / AutoCheckMap=<name> into the slot's cartographer.txt.
    Later runs: the same steps, but a file is copied only when its SHA-256 differs from the one already in the slot,
    so a repeat check re-stages just what changed (the DLL, the tables, the saves).

    All checks (inputs, install.ps1, vanilla tables, slot, save and install-folder guards, no game running in the slot)
    run before the lock; the running-game check runs again right after it. If a step fails after the lock, the error is
    printed with the exact -Restore command; the slot stays locked.
    Every file copied into the slot has its read-only attribute cleared (the vanilla tables are read-only; the source is
    never touched). A levels.txt row past the vanilla rows that is not a Cartographer row gets a warning naming it.

    A manifest (stage_check.json in the slot's test-mod folder) records each file placed (path and SHA-256) and the
    cartographer.txt rows written.

    -Restore first checks that -Owner holds the slot's lock and refuses otherwise, before it touches anything; with no
    manifest it refuses too. It then runs the toolkit's release_install.py restore when the slot holds a release install,
    removes only the files whose SHA-256 still matches the manifest and only the rows it wrote, deletes the manifest and
    unlocks the slot. A file changed since staging stays and is named. Entries -MapItems added to an items.json or
    item-names.json the slot already had are taken out again (the manifest records their exact text). install.ps1 changes inside tables that already
    existed in the slot (not placed by this script) stay.

    test-harness.dll (the d2r-test-harness build, for the game_ctl click and console commands) is copied into the slot's plugins
    folder when the slot has none, from -HarnessDll, and recorded in the manifest so -Restore removes it. A harness DLL the slot
    already has is left alone and not recorded. The owner's game install is never a source.

    Other plugins in the slot's global plugins folder are parked: every .dll there except the staged DLL and test-harness.dll
    moves to plugins-off\staged-<owner>\ (the manifest records each move; -Restore moves it back when its place is empty).
    A PD2 core-rules / core-maps / core-ui left in a test slot made every character save fail ("Character save failed ...
    encoding-error=yes" every 30 s, after "BC_ASSERT nStat" at load) with no Cartographer in the slot; -KeepPlugins skips this.

    -WhatIf prints every action and changes nothing: no lock, no copy, no installer run.
    The game is never launched; the launch command and the log path are printed.

.PARAMETER Slot
    Slot number: 5, 6, 7 or 11 (the slots approved for Cartographer checks). Any other slot is refused.
.PARAMETER Owner
    Lock owner and run id (MODKIT_RUN), for example the name of your lane.
.PARAMETER Dll
    The built cartographer.dll to place in the slot's global plugins folder (ignored by -Restore, which uses the manifest).
.PARAMETER HarnessDll
    The built test-harness.dll to stage when the slot has none. Default: ..\d2r-test-harness\build\Release\test-harness.dll next to the repo.
    When it is missing, one line says so and staging goes on without it.
.PARAMETER Tables
    Folder with maps.txt, cartographer.txt, bosses.txt (any *.txt) copied to excel\d2rloader\cartographer.
.PARAMETER Saves
    Folder with test saves (.d2s and the files next to them) copied to the slot's own save folder. Refused when it is
    inside the owner's real save folders (the vanilla Saved Games root and every mod folder under it), junctions resolved.
.PARAMETER AutoCheckMap
    Writes AutoCheck=1 and AutoCheckMap=<name> into the slot's cartographer.txt (settings of PR #176; unknown settings
    are harmless to a build without them).
.PARAMETER MapItems
    Adds the example map items (examples\first-map\map-items.txt: t20, t21, t22) to the test mod so a map can be opened from its
    item. A map item needs three things in the test mod, and this switch stages all three (test staging only; players get
    no such files): the misc.txt rows (the base row, the Scroll of Identify, with its type changed to cmap, the Map item type install.ps1 adds to itemtypes.txt: an item of type scro or book is named by its spell, not by namestr, and belt 0), copied from the slot or vanilla misc.txt with the overrides applied by column name; never a potion), an entry per
    code in data\hd\items\items.json (the picture; without it the item has no icon), and a name per code in
    data\local\lng\strings\item-names.json and strings-legacy (the tooltip). The mod's own copy of items.json and
    item-names.json replaces the game's whole file, so each is seeded from -VanillaData when the slot lacks it. Then
    in the game: debug_mode, then cheat spawn t20,inv 0 (see docs/cartographer/testing.md, "Map items in a test slot").
.PARAMETER MapItemRows
    Overrides file: a header with code, name, base and the columns to change, one line per item (default: examples\first-map\map-items.txt in the plugin folder).
.PARAMETER CoreUiAutomap
    Optional file to seed d2rloader\core-ui\automap.txt from instead of the one-row default (copy only; replaces an automap.txt this script placed earlier). Never point it
    at slot 1 or the owner's own install.
.PARAMETER VanillaData
    Extracted vanilla data folder holding hd\items\items.json and local\lng\strings\item-names.json (default:
    <toolkit>\d2r-data\93847\data). Copy only, never edited.
.PARAMETER VanillaExcel
    Folder with the vanilla tables the test mod's excel is seeded from (copy only, never edited). Default: the d2r-game
    d2rloader compiler data next to this repo.
.PARAMETER Project
    Project name for the slot table (default: MODKIT_PROJECT, else none).
.PARAMETER Toolkit
    d2rAI-toolkit checkout (default: the d2rAI-toolkit folder next to this repo's folder).
.PARAMETER KeepPlugins
    Leave the other plugins in the slot's plugins folder where they are (the default parks them, see above).
.PARAMETER Restore
    Restore the slot (remove the files and rows recorded in the manifest, only when unchanged) and unlock it.
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
    [string]$HarnessDll,
    [string]$Tables,
    [string]$Saves,
    [string]$AutoCheckMap,
    [switch]$MapItems,
    [string]$MapItemRows,
    [string]$VanillaData,
    [string]$CoreUiAutomap,
    [string]$VanillaExcel,
    [string]$Project = $env:MODKIT_PROJECT,
    [string]$Toolkit,
    [switch]$KeepPlugins,
    [switch]$Restore,
    [switch]$WhatIf
)

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$self = $MyInvocation.MyCommand.Path
$installer = Join-Path $here 'install.ps1'
$latin1 = [System.Text.Encoding]::GetEncoding(28591)

# Slots the owner approved for Cartographer checks. The config says D2RToolsTest owns Slot11; 5, 6 and 7 are approved too.
$AllowedSlots = @(5, 6, 7, 11)
# Tables install.ps1 needs in the test mod's excel folder (the first two are required, the rest only matter for bosses).
# itemstatcost.txt is for the map stats on monsters (without it in the mod they are skipped with a WARN).
$SeedTables = @('levels.txt', 'lvlmaze.txt', 'itemtypes.txt', 'superuniques.txt', 'monstats.txt', 'monumod.txt', 'itemstatcost.txt', 'properties.txt', 'cubemain.txt')
# core-ui (the PD2 core-ui.dll plugin, not D2RLoader itself) reads automap rows for level types 36 and up from d2rloader\core-ui\automap.txt and the
# game asserts at startup without the file, and also when the file has a header but no row (issue #219). Cartographer adds no
# such level type, so the seed is the header plus one row for level type 36 (nothing draws it).
$defaultModRx = '(?m)^\uFEFF?([ \t]*default_mod[ \t]*=[ \t]*")([^"\r\n]*)(")'  # the default_mod line of the slot's d2rloader.toml
$CoreUiAutomapSeed = "LevelType`tTileType`tStyle`tStartSequence`tEndSequence`tCelCount`tCel1`tCel2`tCel3`tCel4`r`n36`t0`t0`t0`t0`t1`t0`t0`t0`t0`r`n"

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
if (-not $VanillaData) { $VanillaData = Join-Path $Toolkit 'd2r-data\93847\data' }
if (-not $MapItemRows) { $MapItemRows = Join-Path (Split-Path -Parent $here) 'examples\first-map\map-items.txt' }
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
real = lambda x: os.path.realpath(x)
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
    $r = & python -I -c 'import os,sys;print(os.path.realpath(sys.argv[1]))' $path
    if ($LASTEXITCODE -ne 0 -or -not $r) { throw "Could not resolve the real path of $path." }
    return ([string]$r).TrimEnd('\')
}
function Test-Under([string]$child, [string]$parent) {
    $c = $child.TrimEnd('\'); $q = $parent.TrimEnd('\')
    return ($c -ieq $q) -or $c.StartsWith($q + '\', [System.StringComparison]::OrdinalIgnoreCase)
}

$modName = $p.modFolder
$modDir = $p.modDir
$excel = Join-Path $modDir "$modName.mpq\data\global\excel"
$cartoTables = Join-Path $excel 'd2rloader\cartographer'
$slotDir = $p.slotDir
$cartoLog = Join-Path $slotDir 'd2rloader\logs\cartographer.log'
$rerun = "& '$self' -Slot $Slot -Owner $Owner -Restore"
$manifestPath = Join-Path $modDir 'stage_check.json'

# Guards (read only; all before the lock). Every place this script writes must be outside the owner's own install,
# and the slot's save folder must not be the owner's own.
foreach ($t in @($slotDir, $modDir, $p.plugins, $p.saveDir, $excel)) {
    if (Test-Under (Get-Real $t) $p.installDir) { throw "Slot $Slot target $t is inside the owner's own install ($($p.installDir)). Refused." }
}
if (Test-Under $p.saveDir $p.ownerSaveDir) { throw "Slot $Slot would use the owner's real save folder ($($p.ownerSaveDir)). Refused." }
if ($p.running.Count -gt 0) { throw "A game is running in slot $Slot (process $(($p.running | ForEach-Object { $_[0] }) -join ', ')). Close it first." }

function Invoke-Toolkit([string]$script, [string[]]$tkArgs) {
    & python -I (Join-Path $testing $script) @tkArgs | Out-Host
    return $LASTEXITCODE
}

function Get-Sha([string]$path) {
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($sha.ComputeHash([System.IO.File]::ReadAllBytes($path))).Replace('-', '') } finally { $sha.Dispose() }
}
# Single quotes inside the Python: Windows PowerShell 5.1 drops double quotes from a native command's arguments.
$pyRunning = @'
import sys, json
sys.path.insert(0, sys.argv[1])
import testing_config as tc, game_slots
print(json.dumps([list(x) for x in game_slots.slot_processes(tc.load(), int(sys.argv[2]))]))
'@
$pyLock = @'
import sys
sys.path.insert(0, sys.argv[1])
import testing_config as tc, game_slots
i = game_slots.lock_info(tc.load(), int(sys.argv[2]))
print('' if i is None else (i.owner or '?'))
'@
function Get-Running {
    $r = & python -I -c $pyRunning $testing $Slot
    if ($LASTEXITCODE -ne 0) { throw "Could not check for a running game in slot $Slot." }
    return @($r | ConvertFrom-Json)
}
# Who holds the slot lock: the owner name, or '' when free.
function Get-LockOwner {
    $r = & python -I -c $pyLock $testing $Slot
    if ($LASTEXITCODE -ne 0) { throw "Could not read the lock of slot $Slot." }
    return ([string]$r).Trim()
}

if ($Restore) {
    Say "Restoring slot $Slot (owner $Owner)."
    # Refuse before touching anything: -Owner must hold the lock, and the manifest must say what this script placed.
    $holder = Get-LockOwner
    if (-not $holder) { throw "Slot $Slot is not locked, so there is nothing of ours to restore. Refused." }
    if ($holder -ne $Owner) { throw "Slot $Slot is locked by '$holder', not by '$Owner'. Refused: nothing was touched." }
    if (-not (Test-Path -PathType Leaf $manifestPath)) { throw "No manifest at ${manifestPath}: this script did not stage the slot, so it will not remove anything. Refused." }
    $man = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
    if ("$($man.owner)" -ne $Owner -or [int]$man.slot -ne $Slot) { throw "The manifest at ${manifestPath} belongs to owner '$($man.owner)', slot $($man.slot). Refused." }
    if ($WhatIf) {
        Plan "python release_install.py status --slot $Slot, then restore when the slot holds a release install"
        foreach ($f in @($man.files)) { Plan "remove $($f.path) when its SHA-256 is still $($f.sha256)" }
        foreach ($r in @($man.rows)) { Plan "remove the row '$($r.row)' from $($r.file)" }
        foreach ($m in @($man.parked)) { Plan "move $($m.to) back to $($m.from) when that place is empty" }
        if ($man.defaultMod) { Plan "set default_mod in $($man.defaultMod.file) back to `"$($man.defaultMod.original)`"" }
        foreach ($e in @($man.entries)) { Plan "remove the JSON entry $((($e.text -split '\r?\n' | ForEach-Object { $_.Trim() }) -join ' ')) from $($e.file)" }
        Plan "delete $manifestPath"
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
    foreach ($f in @($man.files)) {
        if (-not (Test-Path -PathType Leaf $f.path)) { Say "  already gone: $($f.path)"; continue }
        if ((Get-Sha $f.path) -eq $f.sha256) {
            Set-ItemProperty -LiteralPath $f.path -Name IsReadOnly -Value $false
            Remove-Item -LiteralPath $f.path -Force; Say "  removed: $($f.path)"
        } else { Write-Warning "Kept $($f.path): changed since staging (SHA-256 differs from the manifest)." }
    }
    foreach ($m in @($man.parked)) {
        if (-not (Test-Path -LiteralPath $m.to -PathType Leaf)) { Say "  parked plugin already gone: $($m.to)"; continue }
        if (Test-Path -LiteralPath $m.from -PathType Leaf) { Write-Warning "Left $($m.to) parked: $($m.from) exists again. It is no longer tracked; move it back by hand if wanted."; continue }
        Move-Item -LiteralPath $m.to -Destination $m.from; Say "  plugin put back: $($m.from)"
    }
    foreach ($file in @($man.rows | ForEach-Object { $_.file } | Select-Object -Unique)) {
        if (-not (Test-Path -PathType Leaf $file)) { continue }
        $mine = @($man.rows | Where-Object { $_.file -eq $file } | ForEach-Object { $_.row })
        $text = $latin1.GetString([System.IO.File]::ReadAllBytes($file))
        $nl = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
        $lines = @($text -split "\r?\n")
        $left = @(for ($i = 0; $i -lt $lines.Count; $i++) { if ($i -gt 0 -and $mine -contains $lines[$i]) { continue }; $lines[$i] })
        if ($left.Count -ne $lines.Count) {
            Set-ItemProperty -LiteralPath $file -Name IsReadOnly -Value $false
            [System.IO.File]::WriteAllBytes($file, $latin1.GetBytes($left -join $nl))
            Say "  removed $($lines.Count - $left.Count) row(s) from $file"
        }
    }
    # JSON entries added to a file the slot already had: remove each exact text and the comma that joined it, last one first.
    $enc8r = New-Object System.Text.UTF8Encoding($false)
    foreach ($file in @($man.entries | ForEach-Object { $_.file } | Select-Object -Unique)) {
        if (-not (Test-Path -PathType Leaf $file)) { continue }
        $txt = $enc8r.GetString([System.IO.File]::ReadAllBytes($file)); $gone = 0
        $mine = @($man.entries | Where-Object { $_.file -eq $file }); [array]::Reverse($mine)
        foreach ($e in $mine) {
            foreach ($lead in @((',' + $e.nl + $e.text), ($e.nl + $e.text))) {
                $at = $txt.LastIndexOf($lead, [System.StringComparison]::Ordinal)
                if ($at -ge 0) { $txt = $txt.Remove($at, $lead.Length); $gone++; break }
            }
        }
        if ($gone -gt 0) {
            Set-ItemProperty -LiteralPath $file -Name IsReadOnly -Value $false
            [System.IO.File]::WriteAllBytes($file, $enc8r.GetBytes($txt))
            Say "  removed $gone JSON entr$(if ($gone -eq 1) { 'y' } else { 'ies' }) from $file"
        }
    }
    if ($man.defaultMod -and (Test-Path -PathType Leaf $man.defaultMod.file)) {
        $tt = [System.Text.UTF8Encoding]::new($false).GetString([System.IO.File]::ReadAllBytes($man.defaultMod.file))
        $tm = [regex]::Match($tt, $defaultModRx)
        if ($tm.Success -and $tm.Groups[2].Value -eq $modName) {
            Set-ItemProperty -LiteralPath $man.defaultMod.file -Name IsReadOnly -Value $false
            $out = $tt.Substring(0, $tm.Groups[2].Index) + $man.defaultMod.original + $tt.Substring($tm.Groups[2].Index + $tm.Groups[2].Length)
            [System.IO.File]::WriteAllBytes($man.defaultMod.file, [System.Text.UTF8Encoding]::new($false).GetBytes($out))
            Say "  default_mod in $($man.defaultMod.file) set back to `"$($man.defaultMod.original)`""
        } elseif ($tm.Success) {
            Write-Warning "default_mod in $($man.defaultMod.file) is now `"$($tm.Groups[2].Value)`", not the test mod $modName; left as it is (it was `"$($man.defaultMod.original)`" before staging)."
        }
    }
    Remove-Item -LiteralPath $manifestPath -Force
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
if ($CoreUiAutomap) {
    if (-not (Test-Path -PathType Leaf $CoreUiAutomap)) { throw "No file at $CoreUiAutomap (-CoreUiAutomap)." }
    if (Test-Under (Get-Real $CoreUiAutomap) $p.installDir) { throw "-CoreUiAutomap ($CoreUiAutomap) is inside the owner's own install. Refused." }
    if (@(Get-Content -LiteralPath $CoreUiAutomap | Where-Object { $_.Trim() }).Count -lt 2) { throw "-CoreUiAutomap ($CoreUiAutomap) has no data row (header only); core-ui crashes the game at startup without one. Refused." }
}
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
foreach ($name in @('levels.txt', 'lvlmaze.txt', 'itemtypes.txt')) {
    if (-not (Test-Path -PathType Leaf (Join-Path $excel $name)) -and -not (Test-Path -PathType Leaf (Join-Path $VanillaExcel $name))) { $missingSeed += $name }
}
if ($missingSeed.Count -gt 0) { throw "Neither the slot's test mod nor $VanillaExcel has $($missingSeed -join ', '), which install.ps1 requires." }

if ($MapItems) {
    if (-not (Test-Path -PathType Leaf $MapItemRows)) { throw "No map item rows at $MapItemRows. Pass -MapItemRows." }
    foreach ($rel in @('hd\items\items.json', 'local\lng\strings\item-names.json', 'local\lng\strings-legacy\item-names.json')) {
        if (-not (Test-Path -PathType Leaf (Join-Path $VanillaData $rel))) { throw "No vanilla $rel under $VanillaData, which -MapItems seeds the test mod from. Pass -VanillaData." }
    }
    if (-not (Test-Path -PathType Leaf (Join-Path $VanillaExcel 'misc.txt')) -and -not (Test-Path -PathType Leaf (Join-Path $excel 'misc.txt'))) { throw "No misc.txt in the slot's test mod or in $VanillaExcel." }
}

# 1. Lock (never --force).
Say "Slot $Slot, owner $Owner, test mod $modName, slot folder $slotDir"
if ($WhatIf) {
    Plan "python game_slots.py lock $Slot $Owner$(if ($Project) { " --project $Project" })   (refuses slot 1 and a slot held by someone else)"
} else {
    $lockArgs = @('lock', "$Slot", $Owner)
    if ($Project) { $lockArgs += @('--project', $Project) }
    $rc = Invoke-Toolkit 'game_slots.py' $lockArgs
    if ($rc -ne 0) { throw "Could not lock slot $Slot (exit $rc). Not forced: wait for it or pick another slot." }
    # Another lane may have started a game between the first check and the lock.
    $now = Get-Running
    if ($now.Count -gt 0) {
        Write-Host "A game started in slot $Slot (process $(($now | ForEach-Object { $_[0] }) -join ', ')) while it was being locked. Slot $Slot is LOCKED by $Owner; release it with:" -ForegroundColor Red
        Write-Host "  $rerun" -ForegroundColor Red
        throw "A game is running in slot $Slot. Nothing was staged."
    }
}

# The manifest: what this script placed (path, SHA-256) and the cartographer.txt rows it wrote. Hashes are taken when it is saved.
$placed = [ordered]@{}
$rowsWritten = New-Object System.Collections.Generic.List[object]
# JSON entries -MapItems added to an items.json / item-names.json the slot already had (a file this script created is removed whole).
$jsonAdded = New-Object System.Collections.Generic.List[object]
# The slot's d2rloader.toml default_mod before this script set it to the test mod (file and original value).
$defaultMod = $null
# Plugins moved out of the slot's plugins folder (from, to).
$parked = New-Object System.Collections.Generic.List[object]
# Read-only, so -WhatIf loads it too and plans the same replace a real run would make.
if (Test-Path -PathType Leaf $manifestPath) {
    try {
        $old = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
        if ("$($old.owner)" -eq $Owner -and [int]$old.slot -eq $Slot) {
            foreach ($f in @($old.files)) { $placed[$f.path] = $f.sha256 }
            foreach ($r in @($old.rows)) { $rowsWritten.Add(@{ file = $r.file; row = $r.row }) }
            foreach ($e in @($old.entries)) { $jsonAdded.Add(@{ file = $e.file; nl = $e.nl; text = $e.text }) }
            foreach ($m in @($old.parked)) { if (-not @($parked | Where-Object { $_.to -ieq $m.to })) { $parked.Add(@{ from = $m.from; to = $m.to }) } }
            if ($old.defaultMod) { $defaultMod = @{ file = $old.defaultMod.file; original = $old.defaultMod.original } }
        }
    } catch { Write-Warning "Ignoring an unreadable manifest at ${manifestPath}: $($_.Exception.Message)" }
}
function Save-Manifest {
    if ($WhatIf) { return }
    $files = @(foreach ($k in @($placed.Keys)) {
        if (Test-Path -PathType Leaf $k) { @{ path = $k; sha256 = (Get-Sha $k) } }
    })
    $doc = [ordered]@{ owner = $Owner; slot = $Slot; files = $files; rows = @($rowsWritten.ToArray()); entries = @($jsonAdded.ToArray()); parked = @($parked.ToArray()) }
    if ($defaultMod) { $doc['defaultMod'] = $defaultMod }
    $null = New-Item -ItemType Directory -Force (Split-Path $manifestPath -Parent)
    [System.IO.File]::WriteAllText($manifestPath, ($doc | ConvertTo-Json -Depth 5))
}
function Clear-ReadOnly([string]$path) {
    if (Test-Path -PathType Leaf $path) { Set-ItemProperty -LiteralPath $path -Name IsReadOnly -Value $false }
}

# 2. Copy a file only when its SHA-256 differs from the one in the slot.
function Copy-IfChanged([string]$from, [string]$to, [byte[]]$bytes, [switch]$noManifest) {
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
        Clear-ReadOnly $to
        [System.IO.File]::WriteAllBytes($to, $new)
        Clear-ReadOnly $to
        if (-not $noManifest) { $placed[$to] = '' }
    }
}

# Stage test-harness.dll only when the slot has none (a slot's own copy is left alone and not recorded; -Restore removes ours).
function Stage-Harness([string]$pluginsDir, [string]$source) {
    $to = Join-Path $pluginsDir 'test-harness.dll'
    $ours = $placed.Contains($to)
    if ((Test-Path -PathType Leaf $to) -and -not $ours) { Say '  test-harness.dll: the slot already has one, left alone'; return }
    if (-not $source) {
        # Default: ..\d2r-test-harness\build\Release next to the repo; in a worktree also next to the main checkout (git's common dir).
        $rel = 'd2r-test-harness\build\Release\test-harness.dll'
        $repo = Split-Path (Split-Path (Split-Path $here -Parent) -Parent) -Parent
        $roots = @((Split-Path $repo -Parent))
        $common = & git -C $repo rev-parse --path-format=absolute --git-common-dir 2>$null
        if ($LASTEXITCODE -eq 0 -and $common) { $roots += (Split-Path (Split-Path ([string]$common) -Parent) -Parent) }
        $source = Join-Path $roots[0] $rel
        foreach ($r in $roots) { $c = Join-Path $r $rel; if (Test-Path -PathType Leaf $c) { $source = $c; break } }
    }
    if (-not (Test-Path -PathType Leaf $source)) { Say "  test-harness.dll not staged: no build at $source (pass -HarnessDll)"; return }
    Copy-IfChanged $source $to $null
}

try {
    # 3. Stage the test mod: modinfo with the slot's own savepath, and the vanilla tables install.ps1 needs (copies; only files the slot lacks).
    $info = "{`n  `"name`": `"$modName`",`n  `"savepath`": `"$($p.saveName)/`"`n}`n"
    foreach ($mi in @((Join-Path $modDir "$modName.mpq\modinfo.json"), (Join-Path $modDir 'modinfo.json'))) {
        if (-not (Test-Path -PathType Leaf $mi)) {
            Plan "Test mod: write $mi (savepath $($p.saveName)/)"
            if (-not $WhatIf) { $null = New-Item -ItemType Directory -Force (Split-Path $mi -Parent); [System.IO.File]::WriteAllText($mi, $info); Clear-ReadOnly $mi; $placed[$mi] = '' }
        }
    }
    foreach ($name in $SeedTables) {
        $dest = Join-Path $excel $name
        $src = Join-Path $VanillaExcel $name
        if ((Test-Path -PathType Leaf $dest) -or -not (Test-Path -PathType Leaf $src)) { continue }
        Plan "Test mod: seed $name from the vanilla tables (copy; the original is not touched)"
        if (-not $WhatIf) { $null = New-Item -ItemType Directory -Force $excel; Copy-Item -LiteralPath $src -Destination $dest; Clear-ReadOnly $dest; $placed[$dest] = '' }
    }

    # The slot's d2rloader.toml may launch another mod (default_mod = "PD2Resurrected"): point it at the test mod, remember the old value.
    $tomlPath = Join-Path $slotDir 'd2rloader\config\d2rloader.toml'
    if (Test-Path -PathType Leaf $tomlPath) {
        $tt = [System.Text.UTF8Encoding]::new($false).GetString([System.IO.File]::ReadAllBytes($tomlPath))
        $tm = [regex]::Match($tt, $defaultModRx)
        if (-not $tm.Success) { Write-Warning "${tomlPath} has no default_mod line; the game may not load the test mod $modName." }
        elseif ($tm.Groups[2].Value -ne $modName) {
            Plan "d2rloader.toml: default_mod `"$($tm.Groups[2].Value)`" -> `"$modName`" (-Restore puts the old value back)"
            if (-not $WhatIf) {
                if (-not $defaultMod) { $defaultMod = @{ file = $tomlPath; original = $tm.Groups[2].Value } }
                Clear-ReadOnly $tomlPath
                $out = $tt.Substring(0, $tm.Groups[2].Index) + $modName + $tt.Substring($tm.Groups[2].Index + $tm.Groups[2].Length)
                [System.IO.File]::WriteAllBytes($tomlPath, [System.Text.UTF8Encoding]::new($false).GetBytes($out))
            }
        }
    } else { Write-Warning "No ${tomlPath}: the slot's default mod was not checked." }

    # core-ui's automap table (see $CoreUiAutomapSeed). Placed when the mod lacks it; a file this script placed earlier is
    # replaced when -CoreUiAutomap is given or when it has no data row (the old header-only seed crashes the game). A file
    # the script did not place stays. -Restore removes it again (manifest).
    $coreAutomap = Join-Path $excel 'd2rloader\core-ui\automap.txt'
    $haveAutomap = Test-Path -PathType Leaf $coreAutomap
    $seededByUs = $haveAutomap -and $placed.Contains($coreAutomap)
    $noRows = $haveAutomap -and (@(Get-Content -LiteralPath $coreAutomap | Where-Object { $_.Trim() }).Count -lt 2)
    if (-not $haveAutomap -or ($seededByUs -and ($CoreUiAutomap -or $noRows))) {
        $how = if ($CoreUiAutomap) { "copy of $CoreUiAutomap" } else { 'one row, level type 36' }
        Plan "Test mod: seed d2rloader\core-ui\automap.txt ($how)"
        if (-not $WhatIf) {
            $null = New-Item -ItemType Directory -Force (Split-Path $coreAutomap -Parent)
            Clear-ReadOnly $coreAutomap
            if ($CoreUiAutomap) { Copy-Item -LiteralPath $CoreUiAutomap -Destination $coreAutomap -Force } else { [System.IO.File]::WriteAllBytes($coreAutomap, $latin1.GetBytes($CoreUiAutomapSeed)) }
            Clear-ReadOnly $coreAutomap; $placed[$coreAutomap] = ''
        }
    } elseif ($haveAutomap -and $noRows) { Write-Warning "d2rloader\core-ui\automap.txt in the slot has no data row and was not placed by this script: the game may crash at startup. Pass -CoreUiAutomap <file> or remove it." }
    elseif ($haveAutomap -and $CoreUiAutomap) { Write-Warning "d2rloader\core-ui\automap.txt in the slot already has rows and was not placed by this script: -CoreUiAutomap was ignored. Remove that file to use it." }

    # Park every other plugin: a leftover PD2 core-* plugin breaks the character save in a test slot (see the help).
    if (-not $KeepPlugins -and (Test-Path -PathType Container $p.plugins)) {
        $ours = @((Split-Path $Dll -Leaf), 'test-harness.dll')
        # Never park a file this script placed (an earlier staging under another -Dll name); -Restore removes those.
        $stray = @(Get-ChildItem -LiteralPath $p.plugins -File -Filter '*.dll' | Where-Object { $full = $_.FullName; $ours -notcontains $_.Name -and -not @($placed.Keys | Where-Object { $_ -ieq $full }) })
        $parkDir = Join-Path (Split-Path $p.plugins -Parent) "plugins-off\staged-$Owner"
        foreach ($f in $stray) {
            # Never delete anything: take the first free name (<name>, <name>.1, ...) so an earlier parked copy survives.
            $to = Join-Path $parkDir $f.Name
            for ($n = 1; (Test-Path -LiteralPath $to); $n++) { $to = Join-Path $parkDir "$($f.Name).$n" }
            Plan "Park plugin $($f.Name): $($f.FullName) -> $to (-Restore moves it back)"
            if ($WhatIf) { continue }
            $null = New-Item -ItemType Directory -Force $parkDir
            Move-Item -LiteralPath $f.FullName -Destination $to
            $parked.Add(@{ from = $f.FullName; to = $to })
        }
        if ($stray.Count -gt 0) { Save-Manifest }
    }

    # 4. The DLL, the tables, the saves: only what changed.
    Say 'DLL:'
    Copy-IfChanged $Dll (Join-Path $p.plugins (Split-Path $Dll -Leaf)) $null
    Stage-Harness $p.plugins $HarnessDll

    if ($Tables) {
        Say 'Tables:'
        foreach ($f in Get-ChildItem $Tables -File -Filter '*.txt') {
            if ($f.Name -ieq 'cartographer.txt' -and $AutoCheckMap) { continue }  # written with the settings below
            if ($f.Name -ieq 'map-items.txt') { continue }  # the -MapItems overrides, not a game table: -MapItems reads it from the plugin folder
            if ($f.Name -ieq 'misc.txt') { Write-Warning "$($f.FullName): misc.txt is not a Cartographer table (-MapItems adds its rows to the test mod's misc.txt); skipped."; continue }
            Copy-IfChanged $f.FullName (Join-Path $cartoTables $f.Name) $null
        }
    }

    if ($Saves) {
        Say "Saves -> $($p.saveDir):"
        foreach ($f in Get-ChildItem $Saves -File) { Copy-IfChanged $f.FullName (Join-Path $p.saveDir $f.Name) $null }
    }

    # 4b. Map items (-MapItems): misc.txt rows, the HD picture entry and the name strings for each code.
    if ($MapItems) {
        Say 'Map items:'
        $enc8 = New-Object System.Text.UTF8Encoding($false)
        # The overrides file lists code, name and the base item to copy; the other columns are set by name on that row.
        $exRows = @($latin1.GetString([System.IO.File]::ReadAllBytes($MapItemRows)) -split "\r?\n" | Where-Object { $_ -ne '' })
        $exHead = $exRows[0].Split("`t")
        foreach ($need in 'code', 'name', 'base') { if ([array]::IndexOf($exHead, $need) -lt 0) { throw "$MapItemRows needs the column $need." } }
        $items = @(foreach ($line in $exRows[1..($exRows.Count - 1)]) {
            $c = $line.Split("`t"); $v = @{}
            for ($i = 0; $i -lt $exHead.Count; $i++) { $v[$exHead[$i]] = if ($i -lt $c.Count) { $c[$i] } else { '' } }
            @{ code = $v['code']; name = $v['name']; base = $v['base']; key = $(if ($v['namestr']) { $v['namestr'] } else { $v['code'] }); values = $v }
        })
        # misc.txt: seeded from the vanilla table when the slot lacks it; each row is the base row with the overrides applied by column name.
        $misc = Join-Path $excel 'misc.txt'
        if (-not (Test-Path -PathType Leaf $misc)) {
            Plan "Test mod: seed misc.txt from the vanilla tables (copy; the original is not touched)"
            if (-not $WhatIf) { $null = New-Item -ItemType Directory -Force $excel; Copy-Item -LiteralPath (Join-Path $VanillaExcel 'misc.txt') -Destination $misc; Clear-ReadOnly $misc; $placed[$misc] = '' }
        }
        $miscSrc = if (Test-Path -PathType Leaf $misc) { $misc } else { Join-Path $VanillaExcel 'misc.txt' }
        $text = $latin1.GetString([System.IO.File]::ReadAllBytes($miscSrc))
        $mnl = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
        $mlines = [System.Collections.Generic.List[string]]($text -split "\r?\n")
        while ($mlines.Count -gt 0 -and $mlines[$mlines.Count - 1] -eq '') { $mlines.RemoveAt($mlines.Count - 1) }
        $mh = $mlines[0].Split("`t"); $mc = [array]::IndexOf($mh, 'code')
        $vendorCols = @($mh | Where-Object { $_ -cmatch '^[A-Z][a-z]+(Magic)?M(in|ax)$' -and $_ -cnotmatch '^TMog' })
        $added = New-Object System.Collections.Generic.List[string]
        foreach ($it in $items) {
            $have = @($mlines | Select-Object -Skip 1 | Where-Object { $_.Split("`t")[$mc] -ceq $it.code })
            $oldAt = -1
            if ($have.Count -gt 0) {
                # A slot staged before the Map item type has the row with type scro (or book): the game names it "Scroll of Town Portal". Replace that row; any other row is left alone.
                $tc = [array]::IndexOf($mh, 'type')
                $oldCells = $have[0].Split("`t")
                if ($tc -ge 0 -and $it.values.ContainsKey('type') -and $it.values['type'] -cne $oldCells[$tc] -and $oldCells[$tc] -cin 'scro', 'book') {
                    $oldAt = [array]::IndexOf($mlines, $have[0])
                    Say "  misc.txt row $($it.code) has type $($oldCells[$tc]) from an earlier staging; replaced by the row with type $($it.values['type'])"
                } else {
                    Say "  misc.txt already has a row with code $($it.code); left as it is"
                    # An older row can differ from the overrides (compactsave 1 keeps no stats on the item); say which columns.
                    foreach ($col in $exHead) {
                        $ix = [array]::IndexOf($mh, $col)
                        if ($col -in 'code', 'name', 'base', 'namestr' -or $ix -lt 0) { continue }
                        $have1 = if ($ix -lt $oldCells.Count) { $oldCells[$ix] } else { '' }
                        if ($have1 -cne $it.values[$col]) { Write-Warning "misc.txt row $($it.code): $col is '$have1', map-items.txt has '$($it.values[$col])'; the row is kept as it is." }
                    }
                    continue
                }
            }
            $baseRow = @($mlines | Select-Object -Skip 1 | Where-Object { $_.Split("`t")[$mc] -ceq $it.base })
            if ($baseRow.Count -eq 0) { throw "misc.txt has no base item '$($it.base)' to copy for $($it.code) (see $MapItemRows)." }
            $cells = [string[]]::new($mh.Count)
            $src = $baseRow[0].Split("`t")
            for ($i = 0; $i -lt $mh.Count; $i++) { $cells[$i] = if ($i -lt $src.Count) { $src[$i] } else { '' } }
            foreach ($col in $exHead) {
                if ($col -eq 'base') { continue }
                $ix = [array]::IndexOf($mh, $col)
                if ($ix -ge 0) { $cells[$ix] = $it.values[$col] } else { Write-Warning "misc.txt has no column '$col'; that override is skipped." }
            }
            foreach ($col in $vendorCols) { $cells[[array]::IndexOf($mh, $col)] = '' }
            $row = $cells -join "`t"
            if ($oldAt -ge 0) { $mlines[$oldAt] = $row } else { $mlines.Add($row) }
            $added.Add($it.code)
            if (-not $WhatIf) { $rowsWritten.Add(@{ file = $misc; row = $row }) }
        }
        if ($added.Count -gt 0) {
            Plan "  misc.txt: add $($added.Count) map item row(s) ($($added -join ', ')), copied from $((($items | ForEach-Object { $_.base }) | Select-Object -Unique) -join ', ')"
            if (-not $WhatIf) { Clear-ReadOnly $misc; [System.IO.File]::WriteAllBytes($misc, $latin1.GetBytes(($mlines -join $mnl) + $mnl)) }
        }
        # Appends text entries before the closing bracket of a JSON array; an empty array gets no leading comma.
        function Add-JsonEntries([string]$json, [string[]]$entries, [string]$nl) {
            $end = $json.LastIndexOf(']'); if ($end -lt 0) { throw 'Not a JSON array.' }
            $body = $json.Substring(0, $end).TrimEnd()
            $sep = if ($body.EndsWith('[')) { $nl } else { ',' + $nl }
            return $body + $sep + ($entries -join (',' + $nl)) + $nl + ']' + $nl
        }
        # items.json: one entry per code, pointing at the picture the base item uses (no entry = no icon).
        $itemsJson = Join-Path $modDir "$modName.mpq\data\hd\items\items.json"
        $vItems = Join-Path $VanillaData 'hd\items\items.json'
        $newJson = $false
        if (-not (Test-Path -PathType Leaf $itemsJson)) {
            Plan "Test mod: seed items.json from the vanilla data (copy; the mod's file replaces the game's whole file)"
            if (-not $WhatIf) { $null = New-Item -ItemType Directory -Force (Split-Path $itemsJson -Parent); Copy-Item -LiteralPath $vItems -Destination $itemsJson; Clear-ReadOnly $itemsJson; $placed[$itemsJson] = ''; $newJson = $true }
        }
        $jsrc = if (Test-Path -PathType Leaf $itemsJson) { $itemsJson } else { $vItems }
        $jt = $enc8.GetString([System.IO.File]::ReadAllBytes($jsrc))
        $jnl = if ($jt.Contains("`r`n")) { "`r`n" } else { "`n" }
        $new = @($items | Where-Object { $jt -notmatch ('"' + [regex]::Escape($_.code) + '"\s*:') } | ForEach-Object {
            $bm = [regex]::Match($jt, '"' + [regex]::Escape($_.base) + '"\s*:\s*\{\s*"asset"\s*:\s*"([^"]+)"')
            if (-not $bm.Success) { throw "items.json has no picture entry for the base item '$($_.base)' (see $MapItemRows)." }
            "  { `"$($_.code)`": { `"asset`": `"$($bm.Groups[1].Value)`" } }" })
        if ($new.Count -gt 0) {
            if (-not $newJson -and -not $placed.Contains($itemsJson) -and (Test-Path -PathType Leaf $itemsJson)) {
                Say "  items.json in the slot was not placed by this script: -Restore removes the entries added to it."
                if (-not $WhatIf) { foreach ($e in $new) { $jsonAdded.Add(@{ file = $itemsJson; nl = $jnl; text = $e }) } }
            }
            Plan "  items.json: add $($new.Count) picture entr$(if ($new.Count -eq 1) { 'y' } else { 'ies' }): $(($new | ForEach-Object { $_.Trim() }) -join ' ')"
            if (-not $WhatIf) { Clear-ReadOnly $itemsJson; [System.IO.File]::WriteAllBytes($itemsJson, $enc8.GetBytes((Add-JsonEntries $jt $new $jnl))) }
        }
        # item-names.json (HD and legacy): a record per code. The game matches string ids by their low 16 bits and a table
        # string reference is two bytes, so (toolkit knowledge/strings.md) ids count up from the toolkit's next valid id
        # (next_string_id.txt, 28106 in 93847), skip every low-16 value any string file already uses (the mod's and the
        # vanilla data's, across both string folders) and never pass 65535.
        $nameFiles = @(
            @{ rel = 'local\lng\strings\item-names.json'; langs = @('enUS', 'zhTW', 'deDE', 'esES', 'frFR', 'itIT', 'koKR', 'plPL', 'esMX', 'jaJP', 'ptBR', 'ruRU', 'zhCN') },
            @{ rel = 'local\lng\strings-legacy\item-names.json'; langs = @('enUS', 'zhTW', 'deDE', 'esES', 'frFR', 'itIT', 'koKR', 'plPL') })
        $startId = 28106
        $nextIdFile = Join-Path $VanillaData 'local\lng\next_string_id.txt'
        if (Test-Path -PathType Leaf $nextIdFile) {
            $m0 = [regex]::Match([System.IO.File]::ReadAllText($nextIdFile), '\d+')
            if ($m0.Success -and [long]$m0.Value -ge 1 -and [long]$m0.Value -le 65535) { $startId = [int]$m0.Value }
        }
        $usedIds = New-Object 'System.Collections.Generic.HashSet[int]'
        foreach ($dirRel in @('local\lng\strings', 'local\lng\strings-legacy')) {
            foreach ($root in @((Join-Path $modDir "$modName.mpq\data"), $VanillaData)) {
                $dir = Join-Path $root $dirRel
                if (-not (Test-Path -PathType Container $dir)) { continue }
                foreach ($jf in Get-ChildItem -LiteralPath $dir -File -Filter '*.json') {
                    foreach ($m in [regex]::Matches($enc8.GetString([System.IO.File]::ReadAllBytes($jf.FullName)), '"id"\s*:\s*(\d+)')) {
                        $null = $usedIds.Add([int](([long]$m.Groups[1].Value) -band 0xFFFF))
                    }
                }
            }
        }
        foreach ($nf in $nameFiles) {
            $rel = $nf.rel
            $target = Join-Path $modDir "$modName.mpq\data\$rel"
            $vName = Join-Path $VanillaData $rel
            $newFile = $false
            if (-not (Test-Path -PathType Leaf $target)) {
                Plan "Test mod: seed $rel from the vanilla data (copy; the mod's file replaces the game's whole file)"
                if (-not $WhatIf) { $null = New-Item -ItemType Directory -Force (Split-Path $target -Parent); Copy-Item -LiteralPath $vName -Destination $target; Clear-ReadOnly $target; $placed[$target] = ''; $newFile = $true }
            }
            $nsrc = if (Test-Path -PathType Leaf $target) { $target } else { $vName }
            $st = $enc8.GetString([System.IO.File]::ReadAllBytes($nsrc))
            $snl = if ($st.Contains("`r`n")) { "`r`n" } else { "`n" }
            $next = $startId - 1
            $firstId = 0
            $recs = @(foreach ($it in $items) {
                if ($st -match ('"Key"\s*:\s*"' + [regex]::Escape($it.key) + '"')) { continue }
                do { $next++ } while ($usedIds.Contains($next) -and $next -le 65535)
                if ($next -gt 65535) { throw "No free string id left below 65536 for '$($it.key)' (counted up from $startId, skipping the low-16 values already used)." }
                if ($firstId -eq 0) { $firstId = $next }
                $label = $it.name -replace '[\\"]', ''
                $pairs = @($nf.langs | ForEach-Object { "    `"$_`": `"$label`"" }) -join (',' + $snl)
                "  {$snl    `"id`": $next,$snl    `"Key`": `"$($it.key)`",$snl$pairs$snl  }"
            })
            if ($recs.Count -gt 0) {
                if (-not $newFile -and -not $placed.Contains($target) -and (Test-Path -PathType Leaf $target)) {
                    Say "  $rel in the slot was not placed by this script: -Restore removes the names added to it."
                    if (-not $WhatIf) { foreach ($e in $recs) { $jsonAdded.Add(@{ file = $target; nl = $snl; text = $e }) } }
                }
                Plan "  ${rel}: add $($recs.Count) name(s) (ids $firstId to $next, $($nf.langs.Count) languages)"
                if (-not $WhatIf) { Clear-ReadOnly $target; [System.IO.File]::WriteAllBytes($target, $enc8.GetBytes((Add-JsonEntries $st $recs $snl))) }
            }
        }
        Say "  In the game: debug_mode, then cheat spawn <code>,inv 0 for $(($items | ForEach-Object { $_.code }) -join ', ')."
    }

    # A levels.txt row past the vanilla rows that is not a Cartographer row changes the id the Maze level gets.
    $lv = Join-Path $excel 'levels.txt'; $lvVanilla = Join-Path $VanillaExcel 'levels.txt'
    if ((Test-Path -PathType Leaf $lv) -and (Test-Path -PathType Leaf $lvVanilla)) {
        $have = @($latin1.GetString([System.IO.File]::ReadAllBytes($lv)) -split "\r?\n" | Where-Object { $_ -ne '' })
        $vanillaCount = @($latin1.GetString([System.IO.File]::ReadAllBytes($lvVanilla)) -split "\r?\n" | Where-Object { $_ -ne '' }).Count
        for ($i = $vanillaCount; $i -lt $have.Count; $i++) {
            $cols = $have[$i].Split("`t")
            if ($cols[0] -notmatch '^Cartographer (Maze|Preset)$') {
                Write-Warning "levels.txt in the slot's test mod has an extra row past the vanilla rows that is not Cartographer's: '$($cols[0])' (id $($cols[2])). The Maze level will get a later id than on a clean slot."
            }
        }
    }

    # 5. install.ps1 on the test mod's excel folder (safe to run twice).
    if ($WhatIf) {
        Plan "powershell install.ps1 -ExcelDir `"$excel`""
    } else {
        $before = @(if (Test-Path $excel) { Get-ChildItem $excel -Recurse -File | ForEach-Object { $_.FullName } })
        try { & $installer -ExcelDir $excel } finally {
            # Files install.ps1 created (backups, slots.txt) are recorded too; changes it made inside tables this script
            # placed are covered by the hashes taken at the end.
            if (Test-Path $excel) { foreach ($n in (Get-ChildItem $excel -Recurse -File | ForEach-Object { $_.FullName })) { if ($before -notcontains $n) { $placed[$n] = '' } } }
        }
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
        # The settings file is edited by row (-Restore removes only these rows), so it is not recorded as a placed file
        # unless this script created it.
        $existed = Test-Path -PathType Leaf $dest
        Copy-IfChanged $srcFile $dest ($latin1.GetBytes(($lines -join $nl) + $nl)) -noManifest
        if (-not $WhatIf) {
            if (-not $existed) { $placed[$dest] = '' }
            foreach ($kv in @(@('AutoCheck', '1'), @('AutoCheckMap', $AutoCheckMap))) {
                $row = "$($kv[0])`t$($kv[1])"
                if (-not ($rowsWritten | Where-Object { $_.file -eq $dest -and $_.row -eq $row })) { $rowsWritten.Add(@{ file = $dest; row = $row }) }
            }
        }
    }
    Save-Manifest
} catch {
    try { Save-Manifest } catch { Write-Warning "Could not save the manifest: $($_.Exception.Message)" }
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
Say "When done: $rerun"
