<#
.SYNOPSIS
    Adds Cartographer's map item and map slot level to a mod's tables. Safe to run twice.

.DESCRIPTION
    D2RLoader cannot add rows to the game's tables, so the plugin needs these rows in the mod's own files:
      levels.txt    one new level, "Cartographer Preset", a copy of Act 5 Colossal Summit (the built-in preset map)
      lvlprest.txt  its level preset row (a level made from a preset needs one with its own level id)
      misc.txt      the map item "cmap": a copy of the Scroll of Town Portal with item spell 16
      item-names.json (if the mod has one) the item's name
    It also writes d2rloader\cartographer\slots.txt, which tells the plugin the level id it got.
    Rows are only added when missing, so running it again changes nothing. Restart the game afterwards.

.PARAMETER ModDirectory
    The mod's folder, for example <game>\mods\MyMod. The tables are read from <ModDirectory>\<Mod>.mpq\data.

.PARAMETER ExcelDir
    The excel folder to edit instead of the one under ModDirectory (for a test copy).

.PARAMETER NamesFile
    The item-names.json to edit instead of the one under ModDirectory.

.EXAMPLE
    plugins\cartographer\tools\install.ps1 -ModDirectory "C:\Games\Diablo II Resurrected\mods\MyMod"
#>
[CmdletBinding()]
param(
    [string]$ModDirectory,
    [string]$ExcelDir,
    [string]$NamesFile
)

$ErrorActionPreference = 'Stop'
$LevelName = 'Cartographer Preset'
$ItemCode = 'cmap'
$ItemName = 'Cartographer Map'
$TemplateLevelId = '137'  # Act 5 - Colossal Summit: a preset level, so a copy opens with only the preset's rooms

if (-not $ExcelDir) {
    if (-not $ModDirectory) { throw 'Pass -ModDirectory <game>\mods\<Mod> (or -ExcelDir for a test copy).' }
    $mod = Split-Path $ModDirectory -Leaf
    $data = Join-Path $ModDirectory "$mod.mpq\data"
    $ExcelDir = Join-Path $data 'global\excel'
    if (-not $NamesFile) { $NamesFile = Join-Path $data 'local\lng\strings\item-names.json' }
}
if (-not (Test-Path $ExcelDir)) { throw "No excel folder at $ExcelDir. Is this the mod's folder?" }

$utf8 = New-Object System.Text.UTF8Encoding($false)

# A tab-separated table with a header: lines kept as they are, edited by column name.
function Read-Table([string]$path) {
    if (-not (Test-Path $path)) { throw "Missing $path. This mod does not carry that table, so it cannot be edited here." }
    $text = [System.IO.File]::ReadAllText($path, $utf8)
    $eol = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
    $lines = [System.Collections.Generic.List[string]]($text -split [regex]::Escape($eol))
    $final = $lines.Count -gt 0 -and $lines[$lines.Count - 1] -eq ''
    if ($final) { $lines.RemoveAt($lines.Count - 1) }
    $header = $lines[0] -split "`t"
    [pscustomobject]@{ Path = $path; Eol = $eol; Lines = $lines; Header = $header; FinalEol = $final }
}

function Save-Table($t) {
    $text = ($t.Lines -join $t.Eol)
    if ($t.FinalEol) { $text += $t.Eol }
    [System.IO.File]::WriteAllText($t.Path, $text, $utf8)
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

# --- misc.txt ---------------------------------------------------------------------------------------------------------
$misc = Read-Table (Join-Path $ExcelDir 'misc.txt')
if ((Find-Row $misc 'code' $ItemCode) -ge 0) {
    Write-Host "misc.txt: item '$ItemCode' is already there."
} else {
    $templateRow = Find-Row $misc 'code' 'tsc'
    if ($templateRow -lt 0) { throw 'misc.txt has no Scroll of Town Portal (tsc) to copy.' }
    $cells = Cells $misc $misc.Lines[$templateRow]
    Set-Cell $misc $cells 'name' $ItemName
    Set-Cell $misc $cells 'code' $ItemCode
    Set-Cell $misc $cells 'namestr' $ItemCode
    Set-Cell $misc $cells 'pSpell' '16'
    Set-Cell $misc $cells 'useable' '1'
    Set-Cell $misc $cells 'spawnable' '0'
    $misc.Lines.Add($cells -join "`t")
    Save-Table $misc
    Write-Host "misc.txt: added item '$ItemCode' (item spell 16)."
}

# --- item-names.json (best effort: the item still works without a name, it just shows the key) -------------------------
if ($NamesFile -and (Test-Path $NamesFile)) {
    $raw = [System.IO.File]::ReadAllText($NamesFile, $utf8)
    if ($raw -match '"Key"\s*:\s*"' + $ItemCode + '"') {
        Write-Host "item-names.json: '$ItemCode' already has a name."
    } else {
        $entries = $raw | ConvertFrom-Json
        $maxNameId = ($entries | ForEach-Object { [int]$_.id } | Measure-Object -Maximum).Maximum
        $entry = [ordered]@{}
        foreach ($p in $entries[0].PSObject.Properties) { $entry[$p.Name] = $ItemName }
        $entry['id'] = $maxNameId + 1
        $entry['Key'] = $ItemCode
        $close = $raw.LastIndexOf(']')
        $before = $raw.Substring(0, $close).TrimEnd()
        $text = $before + ",`n  " + ($entry | ConvertTo-Json -Compress) + "`n" + $raw.Substring($close)
        [System.IO.File]::WriteAllText($NamesFile, $text, $utf8)
        Write-Host "item-names.json: named '$ItemCode' '$ItemName'."
    }
} else {
    Write-Host "item-names.json: not found, so the item has no name text (it shows as '$ItemCode')."
}

# --- slots.txt --------------------------------------------------------------------------------------------------------
$slotDir = Join-Path $ExcelDir 'd2rloader\cartographer'
New-Item -ItemType Directory -Force $slotDir | Out-Null
$slotText = "Slot`tLevel`r`nPreset`t$levelId`r`n"
$slotPath = Join-Path $slotDir 'slots.txt'
if (-not (Test-Path $slotPath) -or [System.IO.File]::ReadAllText($slotPath, $utf8) -ne $slotText) {
    [System.IO.File]::WriteAllText($slotPath, $slotText, $utf8)
    Write-Host "slots.txt: Preset is level $levelId."
} else {
    Write-Host 'slots.txt: already up to date.'
}
Write-Host 'Done. Restart the game so it reads the tables.'
