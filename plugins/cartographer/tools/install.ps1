<#
.SYNOPSIS
    Adds Cartographer's Maze level to a mod's tables. Safe to run twice.

.DESCRIPTION
    D2RLoader cannot add rows to the game's tables, so the plugin needs these rows in the mod's own files:
      levels.txt    one new level, "Cartographer Maze": Act 5, DrlgType 1 (a generated maze), LevelType 3 (Cave) or 4 (Crypt),
                    360 x 360, Depend 0, Position 0, OffsetX 6000, no warps; copied from a game maze level of that type, so its
                    LevelName keeps a string
      lvlmaze.txt   its maze row (a maze level needs one with its own level id)
    It also adds or updates the Maze row of d2rloader\cartographer\slots.txt, which tells the plugin the level id it got;
    any other row is kept.
    One Maze level serves one tileset: Cave by default, or Crypt with -Tileset Crypt. Every map in maps.txt must use that tileset.
    It does not add any items: each map is opened by an item the mod already has (the Item column of maps.txt), with
    item spell 16 (pSpell in misc.txt). That is the mod author's to set up.
    Both tables are read and checked before anything is written. Each is backed up once as <name>.bak.
    Rows are only added when missing, so running it again changes nothing once the tables are up to date.
    Rows from an older install ("Cartographer Preset" in levels.txt and lvlprest.txt, the Preset row of slots.txt) are left alone:
    nothing uses them any more and they can be deleted. Restart the game afterwards (a mod that ships .bin tables must have
    them rebuilt from the edited .txt).

.PARAMETER ModDirectory
    The mod's folder, for example <game>\mods\MyMod. The tables are read from <ModDirectory>\<Mod>.mpq\data.

.PARAMETER ExcelDir
    The excel folder to edit instead of the one under ModDirectory (for a test copy).

.PARAMETER Tileset
    Cave (the default) or Crypt: the kind of maze the Maze level builds.

.EXAMPLE
    plugins\cartographer\tools\install.ps1 -ModDirectory "C:\Games\Diablo II Resurrected\mods\MyMod"
#>
[CmdletBinding()]
param(
    [string]$ModDirectory,
    [string]$ExcelDir,
    [ValidateSet('Cave', 'Crypt')][string]$Tileset = 'Cave'
)

$ErrorActionPreference = 'Stop'
$LevelName = 'Cartographer Maze'
$OldLevelName = 'Cartographer Preset'
$LevelType = if ($Tileset -eq 'Crypt') { '4' } else { '3' }  # levels.txt LevelType: 3 Cave, 4 Crypt
$MazeSize = '360'  # SizeX and SizeY in every difficulty: the largest plan is 360 tiles on a side
$ActFive = '4'     # levels.txt Act is zero-based

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

# Sets a column only when the table has it (a column some builds of the table do not carry).
function Set-CellIf($t, $cells, [string]$column, [string]$value) {
    $i = [array]::IndexOf($t.Header, $column)
    if ($i -ge 0) { $cells[$i] = $value }
}

foreach ($name in 'levels.txt', 'lvlmaze.txt') {
    if (-not (Test-Path (Join-Path $ExcelDir $name))) { throw "Missing $(Join-Path $ExcelDir $name). This mod does not carry that table, so nothing was changed." }
}

# --- levels.txt -------------------------------------------------------------------------------------------------------
$levels = Read-Table (Join-Path $ExcelDir 'levels.txt')
$idCol = Column $levels 'Id'
$typeCol = Column $levels 'LevelType'
$existing = Find-Row $levels 'Name' $LevelName
if ($existing -ge 0) {
    $levelId = [int](($levels.Lines[$existing] -split "`t")[$idCol])
    $haveType = (Cells $levels $levels.Lines[$existing])[$typeCol]
    Write-Host "levels.txt: '$LevelName' is already there (level $levelId)."
    if ($haveType -ne $LevelType) {
        Write-Host "levels.txt: note: level $levelId has LevelType $haveType, not $LevelType ($Tileset). One Maze level serves one tileset; change its LevelType by hand to switch."
    }
} else {
    # A game maze level of the wanted type to copy: DrlgType 1, that LevelType, one of the game's own rows (id 137 or below).
    $drlgCol = Column $levels 'DrlgType'
    $widest = [Math]::Max($drlgCol, [Math]::Max($typeCol, $idCol))
    $templateRow = -1
    for ($i = 1; $i -lt $levels.Lines.Count; $i++) {
        $cells = $levels.Lines[$i] -split "`t"
        $parsed = 0
        if ($cells.Count -gt $widest -and $cells[$drlgCol] -eq '1' -and $cells[$typeCol] -eq $LevelType -and
            [int]::TryParse($cells[$idCol], [ref]$parsed) -and $parsed -le 137) { $templateRow = $i; break }
    }
    if ($templateRow -lt 0) { throw "levels.txt has no maze level with DrlgType 1 and LevelType $LevelType ($Tileset) to copy." }
    $templateId = ($levels.Lines[$templateRow] -split "`t")[$idCol]
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
    Set-Cell $levels $cells 'Act' $ActFive
    Set-Cell $levels $cells 'DrlgType' '1'
    Set-Cell $levels $cells 'LevelType' $LevelType
    Set-Cell $levels $cells 'Depend' '0'
    Set-Cell $levels $cells 'OffsetX' '6000'
    Set-Cell $levels $cells 'OffsetY' '0'
    Set-CellIf $levels $cells 'Position' '0'
    for ($n = 0; $n -lt 8; $n++) {
        Set-Cell $levels $cells "Vis$n" '0'
        Set-Cell $levels $cells "Warp$n" '-1'  # 0 crashed the game
    }
    $sized = 0
    foreach ($h in $levels.Header) {
        if ($h -match '^Size[XY]') { Set-Cell $levels $cells $h $MazeSize; $sized++ }
    }
    if ($sized -lt 2) { throw 'levels.txt has no SizeX and SizeY columns.' }
    Set-Cell $levels $cells 'Waypoint' '255'
    # LevelName stays the copied key: it is the string the portal tooltip shows, and a key of our own would have no string ("Missing String").
    $levels.Lines.Add($cells -join "`t")
    Save-Table $levels
    Write-Host "levels.txt: added '$LevelName' as level $levelId ($Tileset, copied from level $templateId)."
}
if ((Find-Row $levels 'Name' $OldLevelName) -ge 0) {
    Write-Host "levels.txt: note: an older install's '$OldLevelName' level is still there. Nothing uses it now; it and its lvlprest.txt row can be deleted."
}

# --- lvlmaze.txt ------------------------------------------------------------------------------------------------------
$mazes = Read-Table (Join-Path $ExcelDir 'lvlmaze.txt')
if ((Find-Row $mazes 'Level' "$levelId") -ge 0) {
    Write-Host "lvlmaze.txt: level $levelId already has its maze row."
} else {
    if ($mazes.Lines.Count -lt 2) { throw 'lvlmaze.txt has no row to copy.' }
    $cells = Cells $mazes $mazes.Lines[1]
    Set-Cell $mazes $cells 'Name' $LevelName
    Set-Cell $mazes $cells 'Level' "$levelId"
    foreach ($h in $mazes.Header) {
        if ($h -match '^Rooms') { Set-Cell $mazes $cells $h '4' }
        elseif ($h -eq 'SizeX' -or $h -eq 'SizeY') { Set-Cell $mazes $cells $h '24' }
        elseif ($h -eq 'Merge') { Set-Cell $mazes $cells $h '500' }
    }
    $mazes.Lines.Add($cells -join "`t")
    Save-Table $mazes
    Write-Host "lvlmaze.txt: added the maze row for level $levelId."
}

# --- write ------------------------------------------------------------------------------------------------------------
Write-Pending

# --- slots.txt --------------------------------------------------------------------------------------------------------
$slotDir = Join-Path $ExcelDir 'd2rloader\cartographer'
New-Item -ItemType Directory -Force $slotDir | Out-Null
$slotPath = Join-Path $slotDir 'slots.txt'
# Existing rows are kept (an older install's Preset row, for one): only the Maze row is added or updated.
$slotLines = [System.Collections.Generic.List[string]]::new()
if (Test-Path $slotPath) {
    foreach ($line in ([System.IO.File]::ReadAllText($slotPath, $utf8) -split "`r?`n")) { if ($line.Trim() -ne '') { $slotLines.Add($line) } }
}
$slotHeader = if ($slotLines.Count -gt 0) { ($slotLines[0] -replace ('^' + [regex]::Escape($bom)), '') -split "`t" } else { @() }
if ($slotHeader.Count -ge 2 -and $slotHeader[0] -eq 'Slot' -and $slotHeader[1] -eq 'Level') {
    $mazeAt = -1
    for ($i = 1; $i -lt $slotLines.Count; $i++) { if (($slotLines[$i] -split "`t")[0] -eq 'Maze') { $mazeAt = $i; break } }
    if ($mazeAt -lt 0) { $slotLines.Add("Maze`t$levelId") }
    else { $slotLines[$mazeAt] = "Maze`t$levelId" }
} else {
    if ($slotLines.Count -gt 0) { Write-Host 'slots.txt: unreadable header, replaced.' }
    $slotLines.Clear(); $slotLines.Add("Slot`tLevel"); $slotLines.Add("Maze`t$levelId")
}
if (($slotLines | Where-Object { ($_ -split "`t")[0] -eq 'Preset' }).Count -gt 0) {
    Write-Host 'slots.txt: note: the Preset row is not used any more (the plugin ignores it); it can be deleted.'
}
$slotText = ($slotLines -join "`r`n") + "`r`n"
if (-not (Test-Path $slotPath) -or [System.IO.File]::ReadAllText($slotPath, $utf8) -ne $slotText) {
    [System.IO.File]::WriteAllText($slotPath, $slotText, $utf8)
    Write-Host "slots.txt: Maze is level $levelId (other rows kept)."
} else {
    Write-Host 'slots.txt: already up to date.'
}
Write-Host 'Done. Restart the game so it reads the tables. If the mod ships .bin table files, rebuild them from the edited .txt (the game ignores the .txt then).'
