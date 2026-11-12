<#
.SYNOPSIS
    Adds Cartographer's map slot level to a mod's tables. Safe to run twice.

.DESCRIPTION
    D2RLoader cannot add rows to the game's tables, so the plugin needs these rows in the mod's own files:
      levels.txt    one new level, "Cartographer Preset", a copy of Act 5 Colossal Summit (the level every map opens through)
      lvlprest.txt  its level preset row (a level made from a preset needs one with its own level id)
    It also adds or updates the Preset row of d2rloader\cartographer\slots.txt, which tells the plugin the level id it got;
    any other row (such as a hand-added Maze row) is kept.
    It does not add any items: each map is opened by an item the mod already has (the Item column of maps.txt), with
    item spell 16 (pSpell in misc.txt). That is the mod author's to set up.
    Both tables are read and checked before anything is written. Each is backed up once as <name>.bak.
    Rows are only added when missing, so running it again changes nothing. Restart the game afterwards (a mod that ships .bin tables must have them rebuilt from the edited .txt).

.PARAMETER ModDirectory
    The mod's folder, for example <game>\mods\MyMod. The tables are read from <ModDirectory>\<Mod>.mpq\data.

.PARAMETER ExcelDir
    The excel folder to edit instead of the one under ModDirectory (for a test copy).

.EXAMPLE
    plugins\cartographer\tools\install.ps1 -ModDirectory "C:\Games\Diablo II Resurrected\mods\MyMod"
#>
[CmdletBinding()]
param(
    [string]$ModDirectory,
    [string]$ExcelDir
)

$ErrorActionPreference = 'Stop'
$LevelName = 'Cartographer Preset'
$TemplateLevelId = '137'  # Act 5 - Colossal Summit: a preset level, so a copy opens with only the preset's rooms

if (-not $ExcelDir) {
    if (-not $ModDirectory) { throw 'Pass -ModDirectory <game>\mods\<Mod> (or -ExcelDir for a test copy).' }
    $mod = Split-Path $ModDirectory -Leaf
    $data = Join-Path $ModDirectory "$mod.mpq\data"
    $ExcelDir = Join-Path $data 'global\excel'
}
if (-not (Test-Path $ExcelDir)) { throw "No excel folder at $ExcelDir. Is this the mod's folder?" }

# Tables are read and written byte for byte (Latin-1 maps every byte to one char), so text the installer does not
# touch, in any code page, is kept exactly as it was.
$bytes = [System.Text.Encoding]::GetEncoding(28591)
$utf8 = New-Object System.Text.UTF8Encoding($false)
$pending = [System.Collections.Generic.List[object]]::new()
$bom = [string][char]0xEF + [char]0xBB + [char]0xBF

# A tab-separated table with a header: lines kept as they are, edited by column name.
function Read-Table([string]$path) {
    if (-not (Test-Path $path)) { throw "Missing $path. This mod does not carry that table, so it cannot be edited here." }
    $text = $bytes.GetString([System.IO.File]::ReadAllBytes($path))
    $eol = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
    $lines = [System.Collections.Generic.List[string]]($text -split [regex]::Escape($eol))
    $final = $lines.Count -gt 0 -and $lines[$lines.Count - 1] -eq ''
    if ($final) { $lines.RemoveAt($lines.Count - 1) }
    if ($lines.Count -eq 0 -or $lines[0] -eq '') { throw "$path is empty." }
    $header = ($lines[0] -replace ('^' + [regex]::Escape($bom)), '') -split "`t"
    [pscustomobject]@{ Path = $path; Eol = $eol; Lines = $lines; Header = $header; FinalEol = $final }
}

# Queues the table; nothing is written until every table has been read and checked.
function Save-Table($t) { $pending.Add($t) }

function Write-Pending {
    foreach ($t in $pending) {
        $bak = "$($t.Path).bak"
        if (-not (Test-Path $bak)) { Copy-Item $t.Path $bak }  # the original, once
        $text = ($t.Lines -join $t.Eol)
        if ($t.FinalEol) { $text += $t.Eol }
        [System.IO.File]::WriteAllBytes($t.Path, $bytes.GetBytes($text))
    }
}

function Column($t, [string]$name) {
    $i = [array]::IndexOf($t.Header, $name)
    if ($i -lt 0) { throw "$(Split-Path $t.Path -Leaf) has no '$name' column." }
    $i
}

function Cells($t, [string]$line) {
    $cells = $line -split "`t"
    if ($cells.Count -lt $t.Header.Count) { $cells += @('') * ($t.Header.Count - $cells.Count) }
    ,$cells
}

function Find-Row($t, [string]$column, [string]$value) {
    $c = Column $t $column
    for ($i = 1; $i -lt $t.Lines.Count; $i++) {
        $cells = $t.Lines[$i] -split "`t"
        if ($cells.Count -gt $c -and $cells[$c] -eq $value) { return $i }
    }
    -1
}

function Set-Cell($t, $cells, [string]$column, [string]$value) {
    $cells[(Column $t $column)] = $value
}

foreach ($name in 'levels.txt', 'lvlprest.txt') {
    if (-not (Test-Path (Join-Path $ExcelDir $name))) { throw "Missing $(Join-Path $ExcelDir $name). This mod does not carry that table, so nothing was changed." }
}

# --- levels.txt -------------------------------------------------------------------------------------------------------
$levels = Read-Table (Join-Path $ExcelDir 'levels.txt')
$existing = Find-Row $levels 'Name' $LevelName
$idCol = Column $levels 'Id'
if ($existing -ge 0) {
    $levelId = [int](($levels.Lines[$existing] -split "`t")[$idCol])
    Write-Host "levels.txt: '$LevelName' is already there (level $levelId)."
} else {
    $templateRow = Find-Row $levels 'Id' $TemplateLevelId
    if ($templateRow -lt 0) { throw "levels.txt has no level $TemplateLevelId (Act 5 - Colossal Summit) to copy." }
    $maxId = 0
    for ($i = 1; $i -lt $levels.Lines.Count; $i++) {
        $cells = $levels.Lines[$i] -split "`t"
        $parsed = 0
        if ($cells.Count -gt $idCol -and [int]::TryParse($cells[$idCol], [ref]$parsed) -and $parsed -gt $maxId) { $maxId = $parsed }
    }
    $levelId = $maxId + 1
    $cells = Cells $levels $levels.Lines[$templateRow]
    Set-Cell $levels $cells 'Name' $LevelName
    Set-Cell $levels $cells 'Id' "$levelId"
    for ($n = 0; $n -lt 8; $n++) {
        Set-Cell $levels $cells "Vis$n" '0'
        Set-Cell $levels $cells "Warp$n" '-1'
    }
    Set-Cell $levels $cells 'Waypoint' '255'
    if ($levels.Header -contains 'LevelName') { Set-Cell $levels $cells 'LevelName' $LevelName }
    $levels.Lines.Add($cells -join "`t")
    Save-Table $levels
    Write-Host "levels.txt: added '$LevelName' as level $levelId."
}

# --- lvlprest.txt -----------------------------------------------------------------------------------------------------
$presets = Read-Table (Join-Path $ExcelDir 'lvlprest.txt')
if ((Find-Row $presets 'LevelId' "$levelId") -ge 0) {
    Write-Host "lvlprest.txt: level $levelId already has its preset row."
} else {
    $templateRow = Find-Row $presets 'LevelId' $TemplateLevelId
    if ($templateRow -lt 0) { throw "lvlprest.txt has no preset for level $TemplateLevelId to copy." }
    $defCol = Column $presets 'Def'
    $maxDef = 0
    for ($i = 1; $i -lt $presets.Lines.Count; $i++) {
        $cells = $presets.Lines[$i] -split "`t"
        $parsed = 0
        if ($cells.Count -gt $defCol -and [int]::TryParse($cells[$defCol], [ref]$parsed) -and $parsed -gt $maxDef) { $maxDef = $parsed }
    }
    $cells = Cells $presets $presets.Lines[$templateRow]
    Set-Cell $presets $cells 'Name' $LevelName
    Set-Cell $presets $cells 'Def' "$($maxDef + 1)"
    Set-Cell $presets $cells 'LevelId' "$levelId"
    $presets.Lines.Add($cells -join "`t")
    Save-Table $presets
    Write-Host "lvlprest.txt: added the preset row for level $levelId (Def $($maxDef + 1))."
}

# --- write ------------------------------------------------------------------------------------------------------------
Write-Pending

# --- slots.txt --------------------------------------------------------------------------------------------------------
$slotDir = Join-Path $ExcelDir 'd2rloader\cartographer'
New-Item -ItemType Directory -Force $slotDir | Out-Null
$slotPath = Join-Path $slotDir 'slots.txt'
# Existing rows are kept (a hand-added Maze row, for one): only the Preset row is added or updated.
$slotLines = [System.Collections.Generic.List[string]]::new()
if (Test-Path $slotPath) {
    foreach ($line in ([System.IO.File]::ReadAllText($slotPath, $utf8) -split "`r?`n")) { if ($line.Trim() -ne '') { $slotLines.Add($line) } }
}
$slotHeader = if ($slotLines.Count -gt 0) { ($slotLines[0] -replace ('^' + [regex]::Escape($bom)), '') -split "`t" } else { @() }
if ($slotHeader.Count -ge 2 -and $slotHeader[0] -eq 'Slot' -and $slotHeader[1] -eq 'Level') {
    $presetAt = -1
    for ($i = 1; $i -lt $slotLines.Count; $i++) { if (($slotLines[$i] -split "`t")[0] -eq 'Preset') { $presetAt = $i; break } }
    if ($presetAt -lt 0) { $slotLines.Add("Preset`t$levelId") }
    else { $slotLines[$presetAt] = "Preset`t$levelId" }
} else {
    if ($slotLines.Count -gt 0) { Write-Host 'slots.txt: unreadable header, replaced.' }
    $slotLines.Clear(); $slotLines.Add("Slot`tLevel"); $slotLines.Add("Preset`t$levelId")
}
$slotText = ($slotLines -join "`r`n") + "`r`n"
if (-not (Test-Path $slotPath) -or [System.IO.File]::ReadAllText($slotPath, $utf8) -ne $slotText) {
    [System.IO.File]::WriteAllText($slotPath, $slotText, $utf8)
    Write-Host "slots.txt: Preset is level $levelId (other rows kept)."
} else {
    Write-Host 'slots.txt: already up to date.'
}
Write-Host 'Done. Restart the game so it reads the tables. If the mod ships .bin table files, rebuild them from the edited .txt (the game ignores the .txt then).'
