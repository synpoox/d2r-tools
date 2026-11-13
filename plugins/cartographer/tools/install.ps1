<#
.SYNOPSIS
    Adds Cartographer's Maze level and its Outdoor, Snow and Siege levels to a mod's tables. Safe to run twice.

.DESCRIPTION
    D2RLoader cannot add rows to the game's tables, so the plugin needs these rows in the mod's own files:
      levels.txt    one new level, "Cartographer Maze": Act 5, DrlgType 1 (a generated maze), LevelType 3 (Cave) or 4 (Crypt),
                    360 x 360, Depend 0, Position 0, OffsetX 3500, no warps; copied from a plain (non-quest) game maze level of that type,
                    so its LevelName keeps a string; Quest 0, LevelGroup Null and no LevelEntry or LevelWarp, so it is in none of
                    the game's level groups; Vis0..Vis7 are 0 (-1 crashes the game). A row from an older install (copied from the Den of Evil) is repaired the same way, including its name string.
      lvlmaze.txt   its maze row (a maze level needs one with its own level id)
      levels.txt    a second new level, "Cartographer Outdoor", for outdoor (Jungle) maps: Act 5 (the outdoor build only runs in Act 5),
                    DrlgType 3 (outdoor), LevelType 21 (Jungle), SizeX and SizeY 384 (12 blocks of 32 tiles, the largest outdoor map),
                    OffsetX 6000 and OffsetY 3000 (clear of every Act 5 level and of the Maze level), no warps; copied from the
                    game's Act 5 outdoor level 111 (a plain, non-quest DrlgType 3 row; the first such row if that one is missing), with Quest 0,
                    LevelGroup Null, no LevelEntry or LevelWarp, SubType and SubTheme -1 (level 111's Siege sub-level set is not carried over), Vis0..Vis7 = 0 (-1 crashes the game), Waypoint 255. An outdoor level needs no
                    lvlprest.txt or lvlmaze.txt row of its own: its pieces are the game's own Jungle presets.
    levels.txt    a third new level, "Cartographer Snow", for Snow (Barricade) maps: the same kind of level with LevelType 31 (the Barricade level type;
                  the Jungle level's 21 cannot serve Snow), 192 x 192 at OffsetX 1500, OffsetY 0, copied from level 117 (Frozen Tundra) or the first plain Act 5
                  outdoor row, with the same Quest, LevelGroup, SubType, Vis and Waypoint values as the Outdoor level.
    levels.txt    a fourth new level (the third outdoor one), "Cartographer Siege", for Siege (Bloody Foothills) maps: LevelType 30 (Act 5 - Siege), SizeX 240 and SizeY 48 (15 strips of
                  16 x 48 tiles, the widest and flattest slot level), at OffsetX 6000, OffsetY 3900, copied from level 110 (Bloody Foothills) or the first plain Act 5
                  outdoor row, with the same Quest, LevelGroup, SubType, Vis and Waypoint values as the Outdoor level.
    It also adds or updates the Maze, Outdoor, OutdoorSnow and OutdoorSiege rows of d2rloader\cartographer\slots.txt, which tell the plugin the level ids they got;
    any other row is kept. Vanilla rows of every table are copied from, never edited.
      superuniques.txt  one row per valid entry of d2rloader\cartographer\bosses.txt (at most 16; a bad entry is reported and skipped),
                    keyed 'cartoboss:<escaped Name>' (never a vanilla row; vanilla rows are copied from, not edited). Old 'cartoboss:' rows are rebuilt each run.
                    The row's Name cell is the string key 'cartographer:boss_<Name>' (ASCII only: any other character becomes _ and its hex code): the plugin registers its text (the entry's Name) while it loads.
    One Maze level serves one tileset: Cave by default, or Crypt with -Tileset Crypt. Every map in maps.txt must use that tileset.
      itemtypes.txt one new item type row, "Map" (code cmap), appended at the end, for the map items: a copy of the Herb row (type herb: a plain
                    miscellaneous item, non-quest) with Equiv1 misc, Equiv2 empty, not throwable, not beltable, no auto-stack,
                    and the StorePage of herb (misc; vendors stock an item by the misc.txt vendor columns, not by the type). Rare is on (Magic and Normal off), so the game rolls an item of
                    this type as normal or rare and gives a rare one affixes from magicprefix/magicsuffix/rareprefix/raresuffix rows whose itype is cmap. Maps are meant to be normal, rare or unique; magic maps can still drop for now (Magic 1 would mean always magic, so no column stops them) and a plugin hook to block them is planned. The one row the installer changes
                    after it is added is this type's Magic, Rare and Normal cells: a row from an older install is updated to this. The game names an item of type scro or book after its item spell, not its
                    namestr, so a map item (item spell 16) needs a type of its own. A row with code cmap that is not this one stops the installer
                    before anything is written. Existing rows are never changed (except the Map type's quality cells, above).
      itemstatcost.txt one new stat, "cartographer_map_level", appended (a copy of item_maxhp_percent, 9 bits, shown on the item as "Map Level: N"): a map item's level.
                    The plugin registers the tooltip text itself. A mod without itemstatcost.txt is left out (maps then open at their maps.txt Level).
      itemstatcost.txt also seven monster-side stats, cartographer_mon_life, _damage, _fire_res, _cold_res, _light_res, _poison_res and _speed (signed, shown on the item as "+20% Monster Life" and so on).
                    A map item that carries one gives its monsters the matching vanilla stat; a vanilla stat on a map item is the player's. See docs/cartographer/tables.md.
      properties.txt one new property, "map-level" (a copy of str, stat cartographer_map_level), so a cubemain.txt recipe can set the stat, and one property for each monster-side stat
                    (mon-life, mon-damage, mon-fire-res, mon-cold-res, mon-light-res, mon-poison-res, mon-speed), so an affix or unique row can use them. Left out when the mod has no properties.txt.
    It does not add any items: each map is opened by an item the mod already has (the Item column of maps.txt), with
    item spell 16 (pSpell in misc.txt) and type cmap. That is the mod author's to set up.
    The tables are read and checked before anything is written. Each is backed up once as <name>.bak.
    Rows are only added when missing, so running it again changes nothing once the tables are up to date.
    Rows from an older install ("Cartographer Preset" in levels.txt and lvlprest.txt, the Preset row of slots.txt) are left alone:
    nothing uses them any more and they can be deleted. Restart the game afterwards (a mod that ships .bin tables must have
    them rebuilt from the edited .txt).

.PARAMETER ModDirectory
    The mod's folder, for example <game>\mods\MyMod. The tables are read from <ModDirectory>\<Mod>.mpq\data.

.PARAMETER ExcelDir
    The excel folder to edit instead of the one under ModDirectory (for a test copy).

.PARAMETER MazeOffsetX
    Where the Maze level sits on the Act 5 map: OffsetX (default 3500) and, with -MazeOffsetY (default 0), OffsetY. Likewise -OutdoorOffsetX and
    -OutdoorOffsetY (6000, 3000), -SnowOffsetX and -SnowOffsetY (1500, 0) and -SiegeOffsetX and -SiegeOffsetY (6000, 3900). A level is not added where its rectangle overlaps another
    level of the same act in levels.txt (a mod may have its own custom Act 5 levels): the installer prints the row it hits and adds nothing
    for that level, so pass another spot. The spot is only used when the level is added; a level already in levels.txt stays where it is
    (an overlap is warned about). Only rectangles declared in levels.txt are checked, with a Depend level's position added; rows with
    no size (-1) are not. Offsets must be 0 or more, and offset plus level size at most 13107.

.PARAMETER Tileset
    Cave (the default) or Crypt: the kind of maze the Maze level builds.

.EXAMPLE
    plugins\cartographer\tools\install.ps1 -ModDirectory "C:\Games\Diablo II Resurrected\mods\MyMod"
#>
[CmdletBinding()]
param(
    [string]$ModDirectory,
    [string]$ExcelDir,
    [ValidateSet('Cave', 'Crypt')][string]$Tileset = 'Cave',
    [int]$MazeOffsetX = 3500,
    [int]$MazeOffsetY = 0,
    [int]$OutdoorOffsetX = 6000,
    [int]$OutdoorOffsetY = 3000,
    [int]$SnowOffsetX = 1500,
    [int]$SnowOffsetY = 0,
    [int]$SiegeOffsetX = 6000,
    [int]$SiegeOffsetY = 3900
)

$ErrorActionPreference = 'Stop'
$LevelName = 'Cartographer Maze'
$OldLevelName = 'Cartographer Preset'
$LevelType = if ($Tileset -eq 'Crypt') { '4' } else { '3' }  # levels.txt LevelType: 3 Cave, 4 Crypt
$MazeSize = '360'  # SizeX and SizeY in every difficulty: the largest plan is 360 tiles on a side
$ActFive = '4'     # levels.txt Act is zero-based
$NoGroup = 'Null'  # levels.txt LevelGroup of level 0: the group that counts toward no Act 1-5 level
# One row per outdoor tileset. Tileset, Slot, LevelName and LevelType are the same rows as the C++ table (src/outdoor/outdoor_tilesets.h);
# tests/outdoor_tests.cpp reads this block and fails when they differ. Size is SizeX and SizeY in every difficulty (the largest plan of the
# tileset; SizeY, when a row has it, is its height if that differs from Size, as for Siege's 240 x 48); Param is the prefix of its -<Param>OffsetX/-<Param>OffsetY script parameters (defaults 6000,3000 and 1500,0); Range is a per-tileset flag, $true for a level whose layer is not the town's 79 (Maze: Cave 2, Crypt 10; Snow: 84), whose map the game saves as 16-bit cells (see Test-AutomapRange); it is not read from the written row's Layer; Template is the Act 5 level row copied (any other plain Act 5 DrlgType 3 row is the fallback).
$OutdoorTilesets = @(
    # Jungle: the largest plan is 12 x 12 blocks of 32 tiles; the offset is clear of every Act 5 level (x up to 5055) and of the Maze level (x 3500..3860, y 0..360). Template 111 is Act 5 - Barricade 1, the outdoor level the spike (issue #89) copied.
    @{ Tileset = 'Jungle'; Slot = 'Outdoor';     LevelName = 'Cartographer Outdoor'; LevelType = '21'; Size = '384'; Param = 'Outdoor'; Template = 111 },
    # Snow: LevelType 31 is Act 5 - Barricade (Frigid Highlands, Arreat Plateau, Frozen Tundra); the Jungle level's 21 cannot serve Snow maps. The largest plan is 12 x 12 cells of 16 tiles; x 1500..1692, y 0..192 is clear of the Maze level (x 3500..3860) and inside the automap range (an older install had it at 6000,3600, out of range). Template 117 is Act 5 - Barricade Snow (Frozen Tundra).
    @{ Tileset = 'Snow';   Slot = 'OutdoorSnow'; LevelName = 'Cartographer Snow';    LevelType = '31'; Size = '192'; Param = 'Snow';    Template = 117; Range = $true },
    # Siege: LevelType 30 is Act 5 - Siege (Bloody Foothills). The widest plan is 15 strips of 16 x 48 tiles: 240 x 48. Its layer is the town's 79 (like Jungle), so its map is never swapped out and its offset needs no automap range. Template 110 is Act 5 - Siege 1.
    @{ Tileset = 'Siege';  Slot = 'OutdoorSiege'; LevelName = 'Cartographer Siege';   LevelType = '30'; Size = '240'; SizeY = '48'; Param = 'Siege';   Template = 110 }
)
# Level positions are u16 subtiles in game (5 per tile): origin plus size must stay within 13107 tiles (cartographer/05-maze-slot.md Q2).
# A level whose layer is not the town's 79 (the Maze level, Snow) is swapped out on a town trip and its revealed map is saved as 16-bit cells
# (x = (tx - ty) * 8, y = (tx + ty) * 4) when the player leaves it: all four corners must be in range or the map is lost (Test-AutomapRange).
function Test-AutomapRange([int]$offX, [int]$offY, [int]$sx, [int]$sy = 0) {
    if ($sy -le 0) { $sy = $sx }
    return ($offX + $sx - $offY) -le 4095 -and ($offY + $sy - $offX) -le 4096 -and ($offX + $offY + $sx + $sy) -le 8191
}
$AutomapHint = 'keep OffsetX minus OffsetY within -3736..3735 and OffsetX plus OffsetY at most 7471 for a 360-tile level (smaller levels have a little more room)'
foreach ($p in @(@(@{ Param = 'Maze'; Size = [int]$MazeSize; SizeY = [int]$MazeSize; Range = $true }) + @($OutdoorTilesets | ForEach-Object { @{ Param = $_.Param; Size = [int]$_.Size; SizeY = $(if ($_.SizeY) { [int]$_.SizeY } else { [int]$_.Size }); Range = [bool]$_.Range } }))) {
    $x = Get-Variable -Name "$($p.Param)OffsetX" -ValueOnly; $y = Get-Variable -Name "$($p.Param)OffsetY" -ValueOnly
    if ($x -lt 0 -or $y -lt 0 -or $x + $p.Size -gt 13107 -or $y + $p.SizeY -gt 13107) { throw "-$($p.Param)OffsetX and -$($p.Param)OffsetY must be 0 or more and leave room for the $($p.Size) x $($p.SizeY)-tile level (offset plus size at most 13107) (got $x, $y)." }
    if ($p.Range -and -not (Test-AutomapRange $x $y $p.Size $p.SizeY)) { throw "-$($p.Param)OffsetX and -$($p.Param)OffsetY ($x, $y) put part of the $($p.Size) x $($p.SizeY)-tile level out of the range the game keeps a revealed map in (it would lose its map after a trip to town): $AutomapHint." }
}

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
function Save-Table($t) { if (-not $pending.Contains($t)) { $pending.Add($t) } }

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

# The origin of a level row: its OffsetX/OffsetY plus, when Depend names another level, that level's origin (the game adds them; vanilla does
# this for 26/27 and 32/33). A broken chain (missing level, a loop, a non-number) adds up the offsets of the levels reached before the break (a missing or looping parent adds nothing).
function Get-LevelOrigin($t, $c, [int]$depth = 0) {
    $ox = 0; $oy = 0
    [void][int]::TryParse($c[(Column $t 'OffsetX')], [ref]$ox); [void][int]::TryParse($c[(Column $t 'OffsetY')], [ref]$oy)
    $dc = [array]::IndexOf($t.Header, 'Depend'); $idC = Column $t 'Id'
    $dep = 0
    if ($dc -ge 0 -and $dc -lt $c.Count -and $depth -lt 8 -and [int]::TryParse($c[$dc], [ref]$dep) -and $dep -gt 0) {
        for ($i = 1; $i -lt $t.Lines.Count; $i++) {
            $d = $t.Lines[$i] -split "`t"
            if ($d.Count -gt $idC -and $d[$idC] -eq "$dep") {
                $po = Get-LevelOrigin $t $d ($depth + 1)
                return @(($ox + $po[0]), ($oy + $po[1]))
            }
        }
    }
    @($ox, $oy)
}

# The first other level of the same act whose rectangle (origin, see Get-LevelOrigin, and the largest SizeX/SizeY of the three difficulties) overlaps the
# given square, as text, or '' when none does. The row named $name is the one being checked and is skipped. A row with no size (-1: Frigid Highlands
# and Arreat Plateau) is skipped on purpose: act start-up sets the rectangle of such a row, so its static cells say nothing (cartographer/05-maze-slot.md Q2).
function Find-LevelOverlap($t, [string]$name, [int]$offX, [int]$offY, [int]$size, [int]$sizeY = 0) {
    if ($sizeY -le 0) { $sizeY = $size }
    $nameC = Column $t 'Name'; $actC = Column $t 'Act'; $idC = Column $t 'Id'
    $oxC = Column $t 'OffsetX'; $oyC = Column $t 'OffsetY'
    $sizeCols = @($t.Header | Where-Object { $_ -match '^Size[XY]' } | ForEach-Object { [array]::IndexOf($t.Header, $_) })
    for ($i = 1; $i -lt $t.Lines.Count; $i++) {
        $c = $t.Lines[$i] -split "`t"
        if ($c.Count -le [Math]::Max([Math]::Max($nameC, $actC), [Math]::Max($oxC, $oyC)) -or $c[$nameC] -eq $name -or $c[$actC] -ne $ActFive) { continue }
        $ox = 0; $oy = 0
        if (-not [int]::TryParse($c[$oxC], [ref]$ox) -or -not [int]::TryParse($c[$oyC], [ref]$oy)) { continue }
        $w = 0; $h = 0
        foreach ($sc in $sizeCols) {
            $v = 0
            if ($sc -lt $c.Count -and [int]::TryParse($c[$sc], [ref]$v)) {
                if ($t.Header[$sc] -match '^SizeX') { if ($v -gt $w) { $w = $v } } elseif ($v -gt $h) { $h = $v }
            }
        }
        if ($w -le 0 -or $h -le 0) { continue }
        $origin = Get-LevelOrigin $t $c
        $ox = $origin[0]; $oy = $origin[1]
        if ($offX -lt $ox + $w -and $ox -lt $offX + $size -and $offY -lt $oy + $h -and $oy -lt $offY + $sizeY) {
            $id = if ($c.Count -gt $idC) { $c[$idC] } else { '?' }
            return "'$($c[$nameC])' (level $id, x $ox..$($ox + $w), y $oy..$($oy + $h))"
        }
    }
    ''
}
# The row's own spot overlapping something else: a warning, nothing is moved.
function Warn-LevelOverlap($t, [string]$name, $cells) {
    $ox = 0; $oy = 0; $size = 0; $sizeY = 0
    if ([int]::TryParse($cells[(Column $t 'OffsetX')], [ref]$ox) -and [int]::TryParse($cells[(Column $t 'OffsetY')], [ref]$oy) -and
        [int]::TryParse($cells[(Column $t 'SizeX')], [ref]$size)) {
        if (-not [int]::TryParse($cells[(Column $t 'SizeY')], [ref]$sizeY)) { $sizeY = $size }
        $hit = Find-LevelOverlap $t $name $ox $oy $size $sizeY
        if ($hit) { Write-Host "levels.txt: warning: '$name' (x $ox..$($ox + $size), y $oy..$($oy + $sizeY)) overlaps $hit. It was not moved; change its OffsetX and OffsetY by hand." }
    }
}
# The refusal text for a level that would overlap another.
# An older install put a level where its revealed map is lost after a trip to town (Maze at x 6000, y 0; Snow at 6000, 3600): move the row to its
# default spot, unless that spot overlaps another level (then warn and leave it).
function Move-IntoAutomapRange($t, $cells, [string]$name, [int]$offX, [int]$offY) {
    $ox = 0; $oy = 0; $sx = 0; $sy = 0
    if (-not ([int]::TryParse($cells[(Column $t 'OffsetX')], [ref]$ox) -and [int]::TryParse($cells[(Column $t 'OffsetY')], [ref]$oy) -and
              [int]::TryParse($cells[(Column $t 'SizeX')], [ref]$sx) -and [int]::TryParse($cells[(Column $t 'SizeY')], [ref]$sy))) { return }
    if (Test-AutomapRange $ox $oy $sx $sy) { return }
    $hit = Find-LevelOverlap $t $name $offX $offY $sx $sy
    if ($hit) { Write-Host "levels.txt: warning: '$name' is at x $ox, y $oy, too far out for the game to keep a revealed map after a trip to town, and its new spot ($offX, $offY) overlaps $hit. It was not moved."; return }
    Set-Cell $t $cells 'OffsetX' "$offX"
    Set-Cell $t $cells 'OffsetY' "$offY"
    Write-Host "levels.txt: moved '$name' from ($ox, $oy) to ($offX, $offY), so a revealed map survives a trip to town."
}
function Write-OverlapRefusal([string]$name, [string]$param, [int]$offX, [int]$offY, [int]$size, [string]$hit, [int]$sizeY = 0) {
    if ($sizeY -le 0) { $sizeY = $size }
    Write-Host "levels.txt: Left out '$name': its spot (x $offX..$($offX + $size), y $offY..$($offY + $sizeY)) overlaps $hit."
    Write-Host "levels.txt: to place it, run the installer again with another spot (-${param}OffsetX and -${param}OffsetY; a maze or Snow level must also stay in the automap range: $AutomapHint), or move the other level by hand. Nothing was written for '$name'."
}

foreach ($name in 'levels.txt', 'lvlmaze.txt', 'itemtypes.txt') {
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
    # An older install copied the Den of Evil, so the row kept its quest flag, level group and warp text: repair those.
    $cells = Cells $levels $levels.Lines[$existing]
    Warn-LevelOverlap $levels $LevelName $cells
    $before = $cells -join "`t"
    Set-CellIf $levels $cells 'Quest' '0'
    Set-CellIf $levels $cells 'LevelGroup' $NoGroup
    Set-CellIf $levels $cells 'LevelEntry' ''
    Set-CellIf $levels $cells 'LevelWarp' ''
    for ($n = 0; $n -lt 8; $n++) { Set-CellIf $levels $cells "Vis$n" '0' }  # -1 crashes the game
    Move-IntoAutomapRange $levels $cells $LevelName $MazeOffsetX $MazeOffsetY
    # The Den of Evil copy also kept its name string, so the tooltip read "Den of Evil": use the plain game maze level's string instead.
    $nameCol = [array]::IndexOf($levels.Header, 'LevelName')
    if ($nameCol -ge 0 -and $cells[$nameCol] -eq 'Den of Evil') {
        $drlgCol = Column $levels 'DrlgType'
        $questCol = [array]::IndexOf($levels.Header, 'Quest')
        for ($i = 1; $i -lt $levels.Lines.Count; $i++) {
            $t = $levels.Lines[$i] -split "`t"
            $parsed = 0
            if ($t.Count -gt [Math]::Max([Math]::Max($drlgCol, $typeCol), [Math]::Max($idCol, [Math]::Max($questCol, $nameCol))) -and
                $t[$drlgCol] -eq '1' -and $t[$typeCol] -eq $LevelType -and ($questCol -lt 0 -or $t[$questCol] -in '', '0') -and
                [int]::TryParse($t[$idCol], [ref]$parsed) -and $parsed -le 137) { $cells[$nameCol] = $t[$nameCol]; break }
        }
    }
    if (($cells -join "`t") -ne $before) {
        $levels.Lines[$existing] = $cells -join "`t"
        Save-Table $levels
        Write-Host "levels.txt: repaired level $levelId (no quest, no level group, no entry or warp text, Vis0..Vis7 = 0, plain name string), so it counts toward none of the game's levels."
    }
} else {
    # A game maze level of the wanted type to copy: DrlgType 1, that LevelType, one of the game's own rows (id 137 or below),
    # and not a quest level (the Den of Evil is the only one).
    $drlgCol = Column $levels 'DrlgType'
    $questCol = [array]::IndexOf($levels.Header, 'Quest')
    $widest = [Math]::Max($drlgCol, [Math]::Max($typeCol, [Math]::Max($idCol, $questCol)))
    $templateRow = -1
    for ($i = 1; $i -lt $levels.Lines.Count; $i++) {
        $cells = $levels.Lines[$i] -split "`t"
        $parsed = 0
        if ($cells.Count -gt $widest -and $cells[$drlgCol] -eq '1' -and $cells[$typeCol] -eq $LevelType -and
            ($questCol -lt 0 -or $cells[$questCol] -in '', '0') -and
            [int]::TryParse($cells[$idCol], [ref]$parsed) -and $parsed -le 137) { $templateRow = $i; break }
    }
    if ($templateRow -lt 0) { throw "levels.txt has no maze level with DrlgType 1 and LevelType $LevelType ($Tileset) to copy." }
    $levelId = $null
    $hit = Find-LevelOverlap $levels $LevelName $MazeOffsetX $MazeOffsetY ([int]$MazeSize)
    if ($hit) { Write-OverlapRefusal $LevelName 'Maze' $MazeOffsetX $MazeOffsetY ([int]$MazeSize) $hit }
    else {
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
        Set-Cell $levels $cells 'OffsetX' "$MazeOffsetX"
        Set-Cell $levels $cells 'OffsetY' "$MazeOffsetY"
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
        # Not a quest level, in no vanilla level group (level 0's group), and no entry or warp text of the copied level.
        Set-CellIf $levels $cells 'Quest' '0'
        Set-CellIf $levels $cells 'LevelGroup' $NoGroup
        Set-CellIf $levels $cells 'LevelEntry' ''
        Set-CellIf $levels $cells 'LevelWarp' ''
        # LevelName stays the copied key: it is the string the portal tooltip shows, and a key of our own would have no string ("Missing String").
        $levels.Lines.Add($cells -join "`t")
        Save-Table $levels
        Write-Host "levels.txt: added '$LevelName' as level $levelId ($Tileset, copied from level $templateId)."
    }
}
if ((Find-Row $levels 'Name' $OldLevelName) -ge 0) {
    Write-Host "levels.txt: note: an older install's '$OldLevelName' level is still there. Nothing uses it now; it and its lvlprest.txt row can be deleted."
}

# One new outdoor level (Act 5, DrlgType 3) copied from a plain Act 5 outdoor row; an existing row of that name is only repaired. Returns the level id.
function Ensure-OutdoorLevel([string]$name, [string]$type, [string]$size, [int]$offX, [int]$offY, [int]$wantTemplate, [string]$tilesetName, [string]$param, [string]$sizeY = '', [bool]$range = $false) {
    if ($sizeY -eq '') { $sizeY = $size }
    $existingRow = Find-Row $levels 'Name' $name
    if ($existingRow -ge 0) {
        $newId = [int](($levels.Lines[$existingRow] -split "`t")[$idCol])
        Write-Host "levels.txt: '$name' is already there (level $newId)."
        $cells = Cells $levels $levels.Lines[$existingRow]
        Warn-LevelOverlap $levels $name $cells
        $before = $cells -join "`t"
        Set-CellIf $levels $cells 'Quest' '0'
        Set-CellIf $levels $cells 'LevelGroup' $NoGroup
        Set-CellIf $levels $cells 'LevelEntry' ''
        Set-CellIf $levels $cells 'LevelWarp' ''
        Set-CellIf $levels $cells 'SubType' '-1'
        Set-CellIf $levels $cells 'SubTheme' '-1'
        for ($n = 0; $n -lt 8; $n++) { Set-CellIf $levels $cells "Vis$n" '0' }  # -1 crashes the game
        if ($range) { Move-IntoAutomapRange $levels $cells $name $offX $offY }
        if (($cells -join "`t") -ne $before) {
            $levels.Lines[$existingRow] = $cells -join "`t"
            Save-Table $levels
            Write-Host "levels.txt: repaired level $newId (no quest, no level group, no entry or warp text, no sub-level set, Vis0..Vis7 = 0)."
        }
    } else {
        $drlgCol = Column $levels 'DrlgType'
        $actCol = Column $levels 'Act'
        $questCol = [array]::IndexOf($levels.Header, 'Quest')
        $widest = [Math]::Max([Math]::Max($drlgCol, $actCol), [Math]::Max($idCol, $questCol))
        $templateRow = -1
        for ($i = 1; $i -lt $levels.Lines.Count; $i++) {
            $cells = $levels.Lines[$i] -split "`t"
            $parsed = 0
            if ($cells.Count -gt $widest -and $cells[$drlgCol] -eq '3' -and $cells[$actCol] -eq $ActFive -and
                ($questCol -lt 0 -or $cells[$questCol] -in '', '0') -and
                [int]::TryParse($cells[$idCol], [ref]$parsed) -and $parsed -le 137) {
                if ($templateRow -lt 0) { $templateRow = $i }
                if ($parsed -eq $wantTemplate) { $templateRow = $i; break }
            }
        }
        if ($templateRow -lt 0) { throw 'levels.txt has no Act 5 outdoor level (DrlgType 3) to copy.' }
        $hit = Find-LevelOverlap $levels $name $offX $offY ([int]$size) ([int]$sizeY)
        if ($hit) { Write-OverlapRefusal $name $param $offX $offY ([int]$size) $hit ([int]$sizeY); return $null }
        $templateId = ($levels.Lines[$templateRow] -split "`t")[$idCol]
        $maxId = 0
        for ($i = 1; $i -lt $levels.Lines.Count; $i++) {
            $cells = $levels.Lines[$i] -split "`t"
            $parsed = 0
            if ($cells.Count -gt $idCol -and [int]::TryParse($cells[$idCol], [ref]$parsed) -and $parsed -gt $maxId) { $maxId = $parsed }
        }
        $newId = $maxId + 1
        $cells = Cells $levels $levels.Lines[$templateRow]
        Set-Cell $levels $cells 'Name' $name
        Set-Cell $levels $cells 'Id' "$newId"
        Set-Cell $levels $cells 'Act' $ActFive
        Set-Cell $levels $cells 'DrlgType' '3'
        Set-Cell $levels $cells 'LevelType' $type
        Set-Cell $levels $cells 'Depend' '0'
        Set-Cell $levels $cells 'OffsetX' "$offX"
        Set-Cell $levels $cells 'OffsetY' "$offY"
        Set-CellIf $levels $cells 'Position' '0'
        Set-CellIf $levels $cells 'SubType' '-1'    # the template's sub-level set (decoration) is not carried over
        Set-CellIf $levels $cells 'SubTheme' '-1'
        for ($n = 0; $n -lt 8; $n++) {
            Set-Cell $levels $cells "Vis$n" '0'
            Set-Cell $levels $cells "Warp$n" '-1'
        }
        $sized = 0
        foreach ($h in $levels.Header) {
            if ($h -match '^SizeX') { Set-Cell $levels $cells $h $size; $sized++ }
            elseif ($h -match '^SizeY') { Set-Cell $levels $cells $h $sizeY; $sized++ }
        }
        if ($sized -lt 2) { throw 'levels.txt has no SizeX and SizeY columns.' }
        Set-Cell $levels $cells 'Waypoint' '255'
        Set-CellIf $levels $cells 'Quest' '0'
        Set-CellIf $levels $cells 'LevelGroup' $NoGroup
        Set-CellIf $levels $cells 'LevelEntry' ''
        Set-CellIf $levels $cells 'LevelWarp' ''
        $levels.Lines.Add($cells -join "`t")
        Save-Table $levels
        Write-Host "levels.txt: added '$name' as level $newId ($tilesetName, copied from level $templateId)."
    }
    $newId
}

# --- levels.txt: one level per outdoor tileset (Jungle, Snow, Siege) -----------------------------------------------------------
$outdoorIds = @{}
foreach ($t in $OutdoorTilesets) {
    $offX = Get-Variable -Name "$($t.Param)OffsetX" -ValueOnly
    $offY = Get-Variable -Name "$($t.Param)OffsetY" -ValueOnly
    $outdoorIds[$t.Slot] = Ensure-OutdoorLevel $t.LevelName $t.LevelType $t.Size $offX $offY $t.Template $t.Tileset $t.Param $(if ($t.SizeY) { $t.SizeY } else { $t.Size }) ([bool]$t.Range)
}

# --- itemtypes.txt: the Map item type ---------------------------------------------------------------------------------
# A copy of the Herb row (the vanilla non-quest miscellaneous type), appended once. Equiv1 misc is what Herb, Scroll and Key use: the game treats
# the item as plain miscellaneous. It is not 'scro' or 'book' (so it is not a scroll: not a Town Portal scroll, and the game
# does not name it after its item spell), not 'ques' and not a potion type (no belt). Existing rows are never changed (except the Map type's quality cells, repaired below).
$MapTypeCode = 'cmap'
$MapTypeName = 'Map'
$itemTypes = Read-Table (Join-Path $ExcelDir 'itemtypes.txt')
$itCode = Column $itemTypes 'Code'; $itName = Column $itemTypes 'ItemType'
$mapTypeRow = -1
for ($i = 1; $i -lt $itemTypes.Lines.Count; $i++) {
    $c = $itemTypes.Lines[$i] -split "`t"
    if ($c.Count -gt $itCode -and $c[$itCode] -ieq $MapTypeCode) {
        if ($c.Count -gt $itName -and $c[$itName] -ceq $MapTypeName) { $mapTypeRow = $i; break }
        throw "itemtypes.txt: the code '$MapTypeCode' is already used by another item type ('$($c[$itName])', row $i). Cartographer's map items need the type '$MapTypeCode'. Nothing was changed; rename or remove that row, or use a mod without it."
    }
}
if ($mapTypeRow -ge 0) {
    Write-Host "itemtypes.txt: '$MapTypeName' ($MapTypeCode) is already there."
} else {
    $herbRow = Find-Row $itemTypes 'Code' 'herb'
    if ($herbRow -lt 0) { throw "itemtypes.txt has no 'herb' row to copy for the '$MapTypeName' type." }
    $cells = Cells $itemTypes $itemTypes.Lines[$herbRow]
    Set-Cell $itemTypes $cells 'ItemType' $MapTypeName
    Set-Cell $itemTypes $cells 'Code' $MapTypeCode
    Set-Cell $itemTypes $cells 'Equiv1' 'misc'
    Set-Cell $itemTypes $cells 'Equiv2' ''
    foreach ($col in 'Repair', 'Body', 'Throwable', 'Reload', 'ReEquip', 'AutoStack', 'Normal', 'Beltable') { Set-CellIf $itemTypes $cells $col '0' }
    foreach ($col in 'Shoots', 'Quiver', 'BodyLoc1', 'BodyLoc2') { Set-CellIf $itemTypes $cells $col '' }
    # Rare on, Magic and Normal off (Normal on forces every item Normal). Magic maps can still drop: Magic 1 means always magic, so no column blocks them; a plugin hook is planned.
    Set-CellIf $itemTypes $cells 'Rare' '1'
    Set-CellIf $itemTypes $cells 'Magic' '0'
    $itemTypes.Lines.Add($cells -join "`t")
    Save-Table $itemTypes
    Write-Host "itemtypes.txt: added the item type '$MapTypeName' ($MapTypeCode, copied from herb): plain miscellaneous, Rare on, not throwable, not beltable, no auto-stack, StorePage misc as herb."
}
# The plugin's own row: bring a row from an older install (Magic and Rare empty) up to date, so maps can roll rare.
if ($mapTypeRow -ge 0) {
    $cells = Cells $itemTypes $itemTypes.Lines[$mapTypeRow]
    $before = $cells -join "`t"
    Set-CellIf $itemTypes $cells 'Rare' '1'
    Set-CellIf $itemTypes $cells 'Magic' '0'
    Set-CellIf $itemTypes $cells 'Normal' '0'
    if (($cells -join "`t") -ne $before) {
        $itemTypes.Lines[$mapTypeRow] = $cells -join "`t"
        Save-Table $itemTypes
        Write-Host "itemtypes.txt: updated the '$MapTypeName' ($MapTypeCode) row: Rare on, Magic and Normal off."
    }
}

# --- itemstatcost.txt: the map level stat -----------------------------------------------------------------------------
# A map item's level is a stat on the item (src/statplan mapLevelFromItem): a cube recipe or an affix may set it (the cube cannot on a map yet, affixes are untested), and the game's tooltip shows it
# through descfunc 19 (the string is a format with the value) and the plugin's own string key. A copy of the vanilla item_maxhp_percent row
# (a plain unsigned-looking item stat), with the price columns and the stat-link columns emptied, 9 bits saved and sent (0 to 511), appended once.
# The mod may not carry itemstatcost.txt: then the stat is left out and every map opens at its maps.txt Level.
$MapLevelStat = 'cartographer_map_level'
$MapLevelString = 'cartographer:map_level_stat'  # registered by the plugin (src/engine/map_stat_text.cpp); text "Map Level: %d"
$statCostPath = Join-Path $ExcelDir 'itemstatcost.txt'
if (-not (Test-Path $statCostPath)) {
    Write-Host "itemstatcost.txt: this mod does not carry it, so the map level stat '$MapLevelStat' was not added; maps open at their maps.txt Level."
} else {
    $statCost = Read-Table $statCostPath
    if ((Find-Row $statCost 'Stat' $MapLevelStat) -ge 0) {
        Write-Host "itemstatcost.txt: '$MapLevelStat' is already there."
    } else {
        $template = Find-Row $statCost 'Stat' 'item_maxhp_percent'
        if ($template -lt 0) { throw "itemstatcost.txt has no 'item_maxhp_percent' row to copy for the '$MapLevelStat' stat." }
        $cells = Cells $statCost $statCost.Lines[$template]
        $newId = 0
        for ($i = 1; $i -lt $statCost.Lines.Count; $i++) { if ($statCost.Lines[$i].Trim() -ne '') { $newId++ } }
        # 511 ends the stat list in an item save, so a stat id of 511 or more cannot be stored.
        if ($newId -ge 511) { throw "itemstatcost.txt already has $newId stat rows; the new '$MapLevelStat' stat would get id $newId, but an item save cannot hold a stat id of 511 or more." }
        Set-Cell $statCost $cells 'Stat' $MapLevelStat
        Set-CellIf $statCost $cells '*ID' "$newId"
        foreach ($col in 'Signed', 'Send Other', 'Add', 'Multiply', 'ValShift', 'Save Add', '1.09-Save Add', 'op', 'op param', 'op base', 'op stat1', 'op stat2', 'op stat3', 'direct', 'maxstat') { Set-CellIf $statCost $cells $col '' }
        foreach ($col in 'Send Bits', 'Save Bits', '1.09-Save Bits') { Set-CellIf $statCost $cells $col '9' }
        Set-CellIf $statCost $cells 'descpriority' '255'
        Set-CellIf $statCost $cells 'descfunc' '19'
        Set-CellIf $statCost $cells 'descval' ''
        Set-CellIf $statCost $cells 'descstrpos' $MapLevelString
        Set-CellIf $statCost $cells 'descstrneg' $MapLevelString
        foreach ($col in 'dgrp', 'dgrpfunc', 'dgrpval', 'dgrpstrpos', 'dgrpstrneg', 'dgrpstr2', 'descstr2') { Set-CellIf $statCost $cells $col '' }
        $statCost.Lines.Add($cells -join "`t")
        Save-Table $statCost
        Write-Host "itemstatcost.txt: added the stat '$MapLevelStat' (id $newId, copied from item_maxhp_percent): 9 bits, shown on the item as '$MapLevelString'."
    }
}

# --- itemstatcost.txt: the monster-side map stats ----------------------------------------------------------------------
# A monster stat needs a stat of its own: a vanilla one (item_maxhp_percent, fireresist, ...) on a map item is the player's. When the map opens the
# plugin gives the matching vanilla stat to the map's monsters (src/statplan monsterStats()). Each is a copy of the map level row made signed:
# sent in 11 bits and saved in 10 bits with an add of 512 (values -512 to 511), shown on the item through descfunc 19 and the plugin's string key
# (src/mapname MonsterStatTexts). Appended once each; a mod without itemstatcost.txt gets none.
$MonsterStats = @(
    @{ Stat = 'cartographer_mon_life';       Key = 'cartographer:mon_life';       Property = 'mon-life';       Note = 'monster life' },
    @{ Stat = 'cartographer_mon_damage';     Key = 'cartographer:mon_damage';     Property = 'mon-damage';     Note = 'monster damage' },
    @{ Stat = 'cartographer_mon_fire_res';   Key = 'cartographer:mon_fire_res';   Property = 'mon-fire-res';   Note = 'monster fire resistance' },
    @{ Stat = 'cartographer_mon_cold_res';   Key = 'cartographer:mon_cold_res';   Property = 'mon-cold-res';   Note = 'monster cold resistance' },
    @{ Stat = 'cartographer_mon_light_res';  Key = 'cartographer:mon_light_res';  Property = 'mon-light-res';  Note = 'monster lightning resistance' },
    @{ Stat = 'cartographer_mon_poison_res'; Key = 'cartographer:mon_poison_res'; Property = 'mon-poison-res'; Note = 'monster poison resistance' },
    @{ Stat = 'cartographer_mon_speed';      Key = 'cartographer:mon_speed';      Property = 'mon-speed';      Note = 'monster speed' }
)
if (Test-Path $statCostPath) {
    $statCost = Read-Table $statCostPath
    $template = Find-Row $statCost 'Stat' 'item_maxhp_percent'
    if ($template -lt 0) { throw "itemstatcost.txt has no 'item_maxhp_percent' row to copy for the monster stats." }
    $added = 0
    foreach ($m in $MonsterStats) {
        if ((Find-Row $statCost 'Stat' $m.Stat) -ge 0) { continue }
        $cells = Cells $statCost $statCost.Lines[$template]
        $newId = 0
        for ($i = 1; $i -lt $statCost.Lines.Count; $i++) { if ($statCost.Lines[$i].Trim() -ne '') { $newId++ } }
        if ($newId -ge 511) { throw "itemstatcost.txt already has $newId stat rows; the new '$($m.Stat)' stat would get id $newId, but an item save cannot hold a stat id of 511 or more." }
        Set-Cell $statCost $cells 'Stat' $m.Stat
        Set-CellIf $statCost $cells '*ID' "$newId"
        foreach ($col in 'Send Other', 'Add', 'Multiply', 'ValShift', 'op', 'op param', 'op base', 'op stat1', 'op stat2', 'op stat3', 'direct', 'maxstat', 'damagerelated') { Set-CellIf $statCost $cells $col '' }
        Set-CellIf $statCost $cells 'Signed' '1'
        Set-CellIf $statCost $cells 'Send Bits' '11'
        foreach ($col in 'Save Bits', '1.09-Save Bits') { Set-CellIf $statCost $cells $col '10' }
        foreach ($col in 'Save Add', '1.09-Save Add') { Set-CellIf $statCost $cells $col '512' }
        Set-CellIf $statCost $cells 'descpriority' '250'
        Set-CellIf $statCost $cells 'descfunc' '19'
        Set-CellIf $statCost $cells 'descval' ''
        Set-CellIf $statCost $cells 'descstrpos' $m.Key
        Set-CellIf $statCost $cells 'descstrneg' $m.Key
        foreach ($col in 'dgrp', 'dgrpfunc', 'dgrpval', 'dgrpstrpos', 'dgrpstrneg', 'dgrpstr2', 'descstr2') { Set-CellIf $statCost $cells $col '' }
        $statCost.Lines.Add($cells -join "`t")
        $added++
        Write-Host "itemstatcost.txt: added the stat '$($m.Stat)' (id $newId, $($m.Note))."
    }
    if ($added -gt 0) { Save-Table $statCost } else { Write-Host 'itemstatcost.txt: the monster stats are already there.' }
}

# --- properties.txt: the map level property ---------------------------------------------------------------------------
# A cube recipe (cubemain.txt "mod 1") and an affix name a property, not a stat, so the stat gets a property of its own: 'map-level' sets
# cartographer_map_level to the value in the recipe (docs/cartographer/tables.md has an example). A copy of the vanilla 'str' row.
$MapLevelProperty = 'map-level'
$propsPath = Join-Path $ExcelDir 'properties.txt'
if (-not (Test-Path $propsPath)) {
    Write-Host "properties.txt: this mod does not carry it, so the property '$MapLevelProperty' was not added (a cube recipe cannot set the map level)."
} else {
    $props = Read-Table $propsPath
    if ((Find-Row $props 'code' $MapLevelProperty) -ge 0) {
        Write-Host "properties.txt: '$MapLevelProperty' is already there."
    } else {
        $template = Find-Row $props 'code' 'str'
        if ($template -lt 0) { throw "properties.txt has no 'str' row to copy for the '$MapLevelProperty' property." }
        $cells = Cells $props $props.Lines[$template]
        $newId = 0
        for ($i = 1; $i -lt $props.Lines.Count; $i++) { if ($props.Lines[$i].Trim() -ne '') { $newId++ } }
        Set-Cell $props $cells 'code' $MapLevelProperty
        Set-CellIf $props $cells '*Id' "$newId"
        Set-Cell $props $cells 'stat1' $MapLevelStat
        Set-CellIf $props $cells '*Tooltip' 'Map Level #'
        Set-CellIf $props $cells '*Notes' 'Cartographer: sets the map level of a map item'
        $props.Lines.Add($cells -join "`t")
        Save-Table $props
        Write-Host "properties.txt: added the property '$MapLevelProperty' (stat $MapLevelStat, copied from str)."
    }
}

# --- properties.txt: the monster stat properties ----------------------------------------------------------------------
# One property per monster-side stat (a copy of 'str'), so an affix row (magicprefix.txt and the others) or a uniqueitems.txt row can name it:
# the Min and Max of that mod are the stat's value.
if (Test-Path $propsPath) {
    $props = Read-Table $propsPath
    $template = Find-Row $props 'code' 'str'
    if ($template -lt 0) { throw "properties.txt has no 'str' row to copy for the monster stat properties." }
    $added = 0
    foreach ($m in $MonsterStats) {
        if ((Find-Row $props 'code' $m.Property) -ge 0) { continue }
        $cells = Cells $props $props.Lines[$template]
        $newId = 0
        for ($i = 1; $i -lt $props.Lines.Count; $i++) { if ($props.Lines[$i].Trim() -ne '') { $newId++ } }
        Set-Cell $props $cells 'code' $m.Property
        Set-CellIf $props $cells '*Id' "$newId"
        Set-Cell $props $cells 'stat1' $m.Stat
        Set-CellIf $props $cells '*Tooltip' ($m.Note + ' #')
        Set-CellIf $props $cells '*Notes' 'Cartographer: a monster-side map stat'
        $props.Lines.Add($cells -join "`t")
        $added++
        Write-Host "properties.txt: added the property '$($m.Property)' (stat $($m.Stat))."
    }
    if ($added -gt 0) { Save-Table $props } else { Write-Host 'properties.txt: the monster stat properties are already there.' }
}

# --- lvlmaze.txt ------------------------------------------------------------------------------------------------------
$mazes = Read-Table (Join-Path $ExcelDir 'lvlmaze.txt')
if ($null -eq $levelId) {
    Write-Host 'lvlmaze.txt: no Maze level, so no maze row.'
} elseif ((Find-Row $mazes 'Level' "$levelId") -ge 0) {
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

# --- superuniques.txt: one row per bosses.txt entry -------------------------------------------------------------------
# A map's Boss names a bosses.txt entry (d2rloader\cartographer\bosses.txt: Name, Base, Mods, Minions). Each entry gets its own
# superuniques.txt row, keyed 'cartoboss:' plus the escaped Name (Get-BossEscaped) so it can never be a vanilla row. Vanilla rows are copied from, never edited.
# The 'cartoboss:' rows are rebuilt from bosses.txt each run (so a renamed or removed entry loses its old row), and running this
# again changes nothing once they are right. An entry that does not check out is reported and not written.
# At most $MaxBosses entries: the game keeps a once-per-game "already spawned" bit per superuniques row in a bit list of unknown
# length (docs/reverse-engineering/cartographer/01-addresses.md), so the number of added rows stays small.
$BossKeyPrefix = 'cartoboss:'
$MaxBosses = 16
$BossNameKeyPrefix = 'cartographer:'  # the plugin's string namespace (its PluginInfo.id); src/mapname BossRowKey
# The local key is ASCII only: 'boss_' then each byte of the Name, kept when it is A-Z a-z 0-9, else '_' and two uppercase hex digits
# ('Shoreline Warden' -> 'boss_Shoreline_20Warden'). The same rule as src/mapname BossLocalKey. The Name is UTF-8 (a Latin-1 file is converted first) and held one
# char per byte, so a character here is a byte.
function Get-BossEscaped([string]$Name) {
    $sb = [System.Text.StringBuilder]::new()
    foreach ($ch in $Name.ToCharArray()) {
        if (($ch -ge 'A' -and $ch -le 'Z') -or ($ch -ge 'a' -and $ch -le 'z') -or ($ch -ge '0' -and $ch -le '9')) { [void]$sb.Append($ch) }
        else { [void]$sb.Append('_').Append(([int]$ch -band 0xFF).ToString('X2')) }
    }
    $sb.ToString()
}
function Get-BossLocalKey([string]$Name) { 'boss_' + (Get-BossEscaped $Name) }
# Trims only space, tab, CR and LF, like the plugin's Trim (core/text.h). .NET Trim() also strips U+00A0, U+0085, \v and \f, which
# would give the two sides different keys for a Name that ends in one (easy to get in Latin-1).
function Trim-BossCell([string]$Text) { $Text.Trim(' ', "`t", "`r", "`n") }
# Folds only A-Z, like the plugin's Lower (core/text.h). ToLowerInvariant would also fold 'É' to 'é', so 'René' and 'RENÉ' would be
# the same name here but two names in the plugin.
function Get-AsciiLower([string]$Text) { [regex]::Replace($Text, '[A-Z]', { param($m) $m.Value.ToLowerInvariant() }) }
$bossPath = Join-Path $ExcelDir 'd2rloader\cartographer\bosses.txt'
# A missing bosses.txt counts as an empty one: the old 'cartoboss:' rows are still removed.
$bossLines = @()
if (Test-Path $bossPath) {
    # Text tables are UTF-8; a file that is not valid UTF-8 is read as Latin-1 and converted (NormalizeTableText in src/core/text.h does
    # the same, so the escaped key matches). $bossText keeps one char per UTF-8 byte, like the other tables here.
    $bossRaw = [System.IO.File]::ReadAllBytes($bossPath)
    $strictUtf8 = New-Object System.Text.UTF8Encoding($false, $true)
    if ($bossRaw.Length -ge 3 -and $bossRaw[0] -eq 0xEF -and $bossRaw[1] -eq 0xBB -and $bossRaw[2] -eq 0xBF) { $bossRaw = [byte[]]@($bossRaw | Select-Object -Skip 3) }
    try { [void]$strictUtf8.GetString($bossRaw) }
    catch {
        Write-Host 'bosses.txt: not valid UTF-8, so it was read as Latin-1 and converted. Save it as UTF-8 to keep characters outside Latin-1.'
        $bossRaw = $utf8.GetBytes($bytes.GetString($bossRaw))
    }
    $bossText = $bytes.GetString($bossRaw)
    $bossLines = @($bossText -split "`r?`n" | Where-Object { $_.Trim() -ne '' })
}
if (Test-Path (Join-Path $ExcelDir 'superuniques.txt')) {
    $bossHeader = @(if ($bossLines.Count -gt 0) { ($bossLines[0] -replace ('^' + [regex]::Escape($bom)), '') -split "`t" })
    # The plugin matches these headers ignoring case (core/tsv.h), so the installer does too (A-Z only, like the plugin).
    $bossHeaderLower = @($bossHeader | ForEach-Object { Get-AsciiLower $_ })
    $nameAt = [array]::IndexOf($bossHeaderLower, 'name'); $baseAt = [array]::IndexOf($bossHeaderLower, 'base')
    $modsAt = [array]::IndexOf($bossHeaderLower, 'mods'); $minionsAt = [array]::IndexOf($bossHeaderLower, 'minions')
    $superPath = Join-Path $ExcelDir 'superuniques.txt'
    $monstatsPath = Join-Path $ExcelDir 'monstats.txt'
    $supers = $null
    $needed = 'Superunique', 'hcIdx', 'Name', 'Class', 'Mod1', 'Mod2', 'Mod3', 'MinGrp', 'MaxGrp'
    $noEntries = $bossLines.Count -lt 2
    if (-not $noEntries -and ($nameAt -lt 0 -or $baseAt -lt 0)) {
        Write-Host "bosses.txt: needs 'Name' and 'Base' columns; superuniques.txt was not touched."
    } elseif (-not $noEntries -and -not (Test-Path $monstatsPath)) {
        Write-Host 'bosses.txt: this mod has no monstats.txt to check the Base monsters against; superuniques.txt was not touched.'
    } else {
        $supers = Read-Table $superPath
        $missing = @($needed | Where-Object { $supers.Header -notcontains $_ })
        if ($missing.Count -gt 0) {
            Write-Host "bosses.txt: superuniques.txt has no '$($missing -join "', '")' column, so no boss rows were written (the bosses will be spawned as plain monsters of their Base)."
            $supers = $null
        }
    }
    if ($supers) {
        # Ordinal (case-sensitive) sets for Base, like the plugin's Contains; the Name checks fold only A-Z (Get-AsciiLower), like ContainsNoCase.
        $monsterIds = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        $monsterIdsNoCase = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)  # holds Get-AsciiLower ids
        $bossIds = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        $monstats = if (-not $noEntries) { Read-Table $monstatsPath } else { $null }
        $mIdCol = if ($monstats) { Column $monstats 'Id' } else { 0 }
        $mBossCol = if ($monstats) { [array]::IndexOf($monstats.Header, 'boss') } else { -1 }
        for ($i = 1; $monstats -and $i -lt $monstats.Lines.Count; $i++) {
            $c = $monstats.Lines[$i] -split "`t"
            if ($c.Count -gt $mIdCol -and $c[$mIdCol] -ne '') {
                [void]$monsterIds.Add($c[$mIdCol]); [void]$monsterIdsNoCase.Add((Get-AsciiLower $c[$mIdCol]))
                if ($mBossCol -ge 0 -and $c.Count -gt $mBossCol -and $c[$mBossCol] -eq '1') { [void]$bossIds.Add($c[$mIdCol]) }
            }
        }
        $modIds = $null
        $modsPath = Join-Path $ExcelDir 'monumod.txt'
        if (Test-Path $modsPath) {
            $monumod = Read-Table $modsPath
            if ($monumod.Header -contains 'id') {
                $modIds = @{}
                $modCol = Column $monumod 'id'
                for ($i = 1; $i -lt $monumod.Lines.Count; $i++) {
                    $c = $monumod.Lines[$i] -split "`t"
                    $v = 0
                    if ($c.Count -gt $modCol -and [int]::TryParse($c[$modCol], [ref]$v)) { $modIds[$v] = $true }
                }
            }
        }
        $superCol = Column $supers 'Superunique'
        $hcCol = Column $supers 'hcIdx'
        $vanillaSupers = [System.Collections.Generic.Dictionary[string,bool]]::new([StringComparer]::Ordinal)  # a PowerShell @{} folds case (also non-ASCII)
        $kept = [System.Collections.Generic.List[string]]::new()
        $oldRows = [System.Collections.Generic.List[string]]::new()
        $kept.Add($supers.Lines[0])
        $maxHc = 0
        for ($i = 1; $i -lt $supers.Lines.Count; $i++) {
            $c = $supers.Lines[$i] -split "`t"
            $key = if ($c.Count -gt $superCol) { $c[$superCol] } else { '' }
            if ($key.StartsWith($BossKeyPrefix)) { $oldRows.Add($supers.Lines[$i]); continue }
            $kept.Add($supers.Lines[$i])
            if ($key -ne '') { $vanillaSupers[(Get-AsciiLower $key)] = $true }
            $parsed = 0
            if ($c.Count -gt $hcCol -and [int]::TryParse($c[$hcCol], [ref]$parsed) -and $parsed -gt $maxHc) { $maxHc = $parsed }
        }
        $template = Find-Row $supers 'Superunique' 'Bishibosh'
        if ($template -lt 0) { $template = 1 }
        $templateLine = $supers.Lines[$template]
        $rows = [System.Collections.Generic.List[string]]::new()
        $seen = [System.Collections.Generic.Dictionary[string,bool]]::new([StringComparer]::Ordinal)
        for ($b = 1; $b -lt $bossLines.Count; $b++) {
            $bc = $bossLines[$b] -split "`t"
            $bossName = if ($bc.Count -gt $nameAt) { Trim-BossCell $bc[$nameAt] } else { '' }
            $bossBase = if ($bc.Count -gt $baseAt) { Trim-BossCell $bc[$baseAt] } else { '' }
            $skip = {
                param([string]$why)
                Write-Host "superuniques.txt: skipped bosses.txt row $b ('$bossName'): $why"
            }
            if ($bossName -eq '') { & $skip 'the Name cell is empty.'; continue }
            if ($seen.ContainsKey((Get-AsciiLower $bossName))) { & $skip 'a boss with that name is already listed.'; continue }
            if ($vanillaSupers.ContainsKey((Get-AsciiLower $bossName)) -or $monsterIdsNoCase.Contains((Get-AsciiLower $bossName))) { & $skip 'that is the name of a vanilla boss or monster.'; continue }
            if ($bossBase -eq '') { & $skip 'the Base cell is empty.'; continue }
            if (-not $monsterIds.Contains($bossBase)) { & $skip "Base '$bossBase' is not an Id in monstats.txt."; continue }
            if ($bossIds.Contains($bossBase)) { & $skip "Base '$bossBase' is a vanilla boss (its monstats.txt boss column is 1)."; continue }
            $mods = @()
            $bad = ''
            if ($modsAt -ge 0 -and $bc.Count -gt $modsAt -and $bc[$modsAt].Trim() -ne '') {
                foreach ($m in ($bc[$modsAt] -split ',')) {
                    $v = 0
                    if (-not [int]::TryParse($m.Trim(), [ref]$v) -or $v -lt 1 -or $v -gt 255) { $bad = "Mods entry '$($m.Trim())' is not a whole number from 1 to 255."; break }
                    if ($null -ne $modIds -and -not $modIds.ContainsKey($v)) { $bad = "Mods entry '$v' is not an id in monumod.txt."; break }
                    $mods += $v
                }
                if ($bad -eq '' -and $mods.Count -gt 3) { $bad = "Mods has $($mods.Count) entries; the limit is 3." }
            }
            if ($bad -ne '') { & $skip $bad; continue }
            $minions = 0
            if ($minionsAt -ge 0 -and $bc.Count -gt $minionsAt -and $bc[$minionsAt].Trim() -ne '') {
                $v = 0
                if ([int]::TryParse($bc[$minionsAt].Trim(), [ref]$v) -and $v -ge 0 -and $v -le 10) { $minions = $v }
                else { & $skip "Minions '$($bc[$minionsAt].Trim())' is not a whole number from 0 to 10."; continue }
            }
            if ($rows.Count -ge $MaxBosses) { & $skip "bosses.txt may have at most $MaxBosses entries."; continue }
            $seen[(Get-AsciiLower $bossName)] = $true
            $key = $BossKeyPrefix + (Get-BossEscaped $bossName)  # escaped like the string key: ASCII only (BossKey in src/core/tables.cpp)
            # A copy of a vanilla row (read only); every cell that matters is set below.
            $cells = Cells $supers $templateLine
            Set-Cell $supers $cells 'hcIdx' "$($maxHc + $rows.Count + 1)"
            Set-Cell $supers $cells 'Superunique' $key
            # Name is a string key, resolved once when the game loads the table: the plugin registers the text of $BossNameKeyPrefix<local key>
            # (its own string namespace) from bosses.txt while it loads. Vanilla rows and strings are never touched.
            Set-Cell $supers $cells 'Name' ($BossNameKeyPrefix + (Get-BossLocalKey $bossName))
            Set-Cell $supers $cells 'Class' $bossBase
            for ($n = 0; $n -lt 3; $n++) { Set-Cell $supers $cells "Mod$($n + 1)" $(if ($n -lt $mods.Count) { "$($mods[$n])" } else { '0' }) }
            Set-Cell $supers $cells 'MinGrp' "$minions"
            Set-Cell $supers $cells 'MaxGrp' "$minions"
            foreach ($pair in @(@('TC', 'Act 5 Super C'), @('TC(N)', 'Act 5 (N) Super C'), @('TC(H)', 'Act 5 (H) Super C'), @('TC Desecrated', ''), @('TC(N) Desecrated', ''), @('TC(H) Desecrated', 'Act 5 (H) Super C Desecrated'))) {
                if ($supers.Header -contains $pair[0]) { Set-Cell $supers $cells $pair[0] $pair[1] }
            }
            $rows.Add($cells -join "`t")
        }
        if (($oldRows -join "`n") -cne ($rows -join "`n")) {
            foreach ($line in $rows) { $kept.Add($line) }
            $supers.Lines = $kept
            Save-Table $supers
            Write-Host "superuniques.txt: wrote $($rows.Count) boss row(s) (replacing $($oldRows.Count) from an earlier run)."
        } else {
            Write-Host 'superuniques.txt: boss rows already up to date.'
        }
    }
} elseif ($bossLines.Count -ge 2) {
    Write-Host 'bosses.txt: this mod has no superuniques.txt, so the bosses will be spawned as plain monsters of their Base.'
}

# --- write ------------------------------------------------------------------------------------------------------------
# Copied game tables are often Read-only: refuse before anything is written (and before any .bak is made).
$locked = @($pending | ForEach-Object { $_.Path })
$lockedSlots = Join-Path $ExcelDir 'd2rloader\cartographer\slots.txt'
if (Test-Path $lockedSlots) { $locked += $lockedSlots }
$locked = @($locked | Where-Object { (Get-Item -LiteralPath $_).IsReadOnly })
if ($locked.Count -gt 0) { throw ('Nothing was written. Clear the Read-only attribute on these files, then run the installer again:' + [Environment]::NewLine + (($locked | ForEach-Object { "  $_" }) -join [Environment]::NewLine)) }
Write-Pending

# --- slots.txt --------------------------------------------------------------------------------------------------------
$slotDir = Join-Path $ExcelDir 'd2rloader\cartographer'
New-Item -ItemType Directory -Force $slotDir | Out-Null
$slotPath = Join-Path $slotDir 'slots.txt'
# Existing rows are kept (an older install's Preset row, for one): only the Maze row and one row per outdoor tileset (`$OutdoorTilesets) are added or updated.
$slotLines = [System.Collections.Generic.List[string]]::new()
if (Test-Path $slotPath) {
    foreach ($line in ([System.IO.File]::ReadAllText($slotPath, $utf8) -split "`r?`n")) { if ($line.Trim() -ne '') { $slotLines.Add($line) } }
}
$slotHeader = if ($slotLines.Count -gt 0) { ($slotLines[0] -replace ('^' + [regex]::Escape($bom)), '') -split "`t" } else { @() }
if ($slotHeader.Count -ge 2 -and $slotHeader[0] -eq 'Slot' -and $slotHeader[1] -eq 'Level') {
    $slotRows = @(,@('Maze', $levelId))
    foreach ($t in $OutdoorTilesets) { $slotRows += ,@($t.Slot, $outdoorIds[$t.Slot]) }
    foreach ($row in $slotRows) {
        if ($null -eq $row[1]) { continue }  # refused above: keep whatever the row says
        $at = -1
        for ($i = 1; $i -lt $slotLines.Count; $i++) { if (($slotLines[$i] -split "`t")[0] -eq $row[0]) { $at = $i; break } }
        if ($at -lt 0) { $slotLines.Add("$($row[0])`t$($row[1])") }
        else { $slotLines[$at] = "$($row[0])`t$($row[1])" }
    }
} else {
    if ($slotLines.Count -gt 0) { Write-Host 'slots.txt: unreadable header, replaced.' }
    $slotLines.Clear(); $slotLines.Add("Slot`tLevel")
    $slotRows = @(,@('Maze', $levelId))
    foreach ($t in $OutdoorTilesets) { $slotRows += ,@($t.Slot, $outdoorIds[$t.Slot]) }
    foreach ($row in $slotRows) { if ($null -ne $row[1]) { $slotLines.Add("$($row[0])`t$($row[1])") } }
}
if (($slotLines | Where-Object { ($_ -split "`t")[0] -eq 'Preset' }).Count -gt 0) {
    Write-Host 'slots.txt: note: the Preset row is not used any more (the plugin ignores it); it can be deleted.'
}
$slotText = ($slotLines -join "`r`n") + "`r`n"
if (-not (Test-Path $slotPath) -or [System.IO.File]::ReadAllText($slotPath, $utf8) -ne $slotText) {
    [System.IO.File]::WriteAllText($slotPath, $slotText, $utf8)
    $shown = { param($id) if ($null -ne $id) { "is level $id" } else { 'not added' } }
    Write-Host ("slots.txt: Maze $(& $shown $levelId), " + (($OutdoorTilesets | ForEach-Object { "$($_.Slot) $(& $shown $outdoorIds[$_.Slot])" }) -join ', ') + ' (other rows kept).')
} else {
    Write-Host 'slots.txt: already up to date.'
}
Write-Host 'Done. Restart the game so it reads the tables. If the mod ships .bin table files, rebuild them from the edited .txt with D2RLoader and Cartographer active (the game ignores the .txt then).'
