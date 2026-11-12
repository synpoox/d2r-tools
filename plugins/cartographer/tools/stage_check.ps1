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
    run before the lock; the running-game check runs again right after it. If a step fails after the lock, the error is
    printed with the exact -Restore command; the slot stays locked.
    Every file copied into the slot has its read-only attribute cleared (the vanilla tables are read-only; the source is
    never touched). A levels.txt row past the vanilla rows that is not a Cartographer row gets a warning naming it.

    A manifest (stage_check.json in the slot's test-mod folder) records each file placed (path and SHA-256) and the
    cartographer.txt rows written.

    -Restore first checks that -Owner holds the slot's lock and refuses otherwise, before it touches anything; with no
    manifest it refuses too. It then runs the toolkit's release_install.py restore when the slot holds a release install,
    removes only the files whose SHA-256 still matches the manifest and only the rows it wrote, deletes the manifest and
    unlocks the slot. A file changed since staging stays and is named. install.ps1 changes inside tables that already
    existed in the slot (not placed by this script) stay.

    -WhatIf prints every action and changes nothing: no lock, no copy, no installer run.
    The game is never launched; the launch command and the log path are printed.

.PARAMETER Slot
    Slot number: 5, 6, 7 or 11 (the slots approved for Cartographer checks). Any other slot is refused.
.PARAMETER Owner
    Lock owner and run id (MODKIT_RUN), for example the name of your lane.
.PARAMETER Dll
    The built cartographer.dll to place in the slot's global plugins folder (ignored by -Restore, which uses the manifest).
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
$self = $MyInvocation.MyCommand.Path
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
if (-not $WhatIf -and (Test-Path -PathType Leaf $manifestPath)) {
    try {
        $old = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
        if ("$($old.owner)" -eq $Owner -and [int]$old.slot -eq $Slot) {
            foreach ($f in @($old.files)) { $placed[$f.path] = $f.sha256 }
            foreach ($r in @($old.rows)) { $rowsWritten.Add(@{ file = $r.file; row = $r.row }) }
        }
    } catch { Write-Warning "Ignoring an unreadable manifest at ${manifestPath}: $($_.Exception.Message)" }
}
function Save-Manifest {
    if ($WhatIf) { return }
    $files = @(foreach ($k in @($placed.Keys)) {
        if (Test-Path -PathType Leaf $k) { @{ path = $k; sha256 = (Get-Sha $k) } }
    })
    $doc = [ordered]@{ owner = $Owner; slot = $Slot; files = $files; rows = @($rowsWritten.ToArray()) }
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
        Plan "powershell install.ps1 -ExcelDir `"$excel`" -Tileset $Tileset"
    } else {
        $before = @(if (Test-Path $excel) { Get-ChildItem $excel -Recurse -File | ForEach-Object { $_.FullName } })
        try { & $installer -ExcelDir $excel -Tileset $Tileset } finally {
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
