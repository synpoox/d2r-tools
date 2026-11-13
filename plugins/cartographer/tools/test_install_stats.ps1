<#
.SYNOPSIS
    Offline test of install.ps1's itemstatcost.txt and properties.txt rows (#358). Two runs on a scratch copy of small stand-in vanilla tables.

.DESCRIPTION
    Builds an excel folder in the temp folder (levels, lvlmaze, itemtypes, itemstatcost with 368 vanilla stats, properties with 293 vanilla rows),
    runs install.ps1 twice, and checks that the first run wrote every Cartographer stat and property with unique ids in order and the
    second run changed no file. Writes nothing in the repo or the game. Exits 1 on a failure.
#>
$ErrorActionPreference = 'Stop'
$installer = Join-Path $PSScriptRoot 'install.ps1'
$dir = Join-Path ([System.IO.Path]::GetTempPath()) ("cartographer-install-test-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $dir | Out-Null
$failed = 0
function Check([bool]$ok, [string]$what) { if ($ok) { Write-Host "ok   $what" } else { Write-Host "FAIL $what"; $script:failed++ } }
function Write-Tsv([string]$name, [string[]]$header, $rows) {
    $lines = @($header -join "`t") + @($rows | ForEach-Object { $_ -join "`t" })
    [System.IO.File]::WriteAllText((Join-Path $dir $name), (($lines -join "`r`n") + "`r`n"), (New-Object System.Text.UTF8Encoding($false)))
}
function Read-Rows([string]$name) { @(Get-Content (Join-Path $dir $name) | Select-Object -Skip 1 | Where-Object { $_ -ne '' } | ForEach-Object { , ($_ -split "`t") }) }
function Run-Installer { & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $installer -ExcelDir $dir 2>&1 | Out-String }
try {
    $lh = @('Name', 'Id', 'Act', 'DrlgType', 'LevelType', 'Depend', 'OffsetX', 'OffsetY', 'Position', 'SizeX', 'SizeY', 'Quest', 'LevelGroup', 'LevelName', 'LevelEntry', 'LevelWarp', 'SubType', 'SubTheme', 'Waypoint') +
        (0..7 | ForEach-Object { "Vis$_" }) + (0..7 | ForEach-Object { "Warp$_" })
    function Level($name, $id, $act, $drlg, $type) {
        $c = @{ Name = $name; Id = "$id"; Act = "$act"; DrlgType = "$drlg"; LevelType = "$type"; Depend = '0'; OffsetX = '0'; OffsetY = '0'; Position = '0'; SizeX = '48'; SizeY = '48'; Quest = '0'; LevelGroup = 'Null'; LevelName = $name; LevelEntry = ''; LevelWarp = ''; SubType = '-1'; SubTheme = '-1'; Waypoint = '255' }
        , @($lh | ForEach-Object { if ($c.ContainsKey($_)) { $c[$_] } elseif ($_ -like 'Vis*') { '0' } elseif ($_ -like 'Warp*') { '-1' } else { '' } })
    }
    Write-Tsv 'levels.txt' $lh @((Level 'Cave 1' 8 0 1 3), (Level 'Crypt 1' 9 0 1 4), (Level 'Bloody Foothills' 110 4 3 30), (Level 'Barricade 1' 111 4 3 21), (Level 'Frozen Tundra' 117 4 3 31), (Level 'Last' 136 4 3 21))
    Write-Tsv 'lvlmaze.txt' @('Name', 'Level', 'Rooms', 'SizeX', 'SizeY', 'Merge') @(, @('Cave 1', '8', '3', '10', '10', '0'))
    $ih = @('ItemType', 'Code', 'Equiv1', 'Equiv2', 'Repair', 'Body', 'Throwable', 'Reload', 'ReEquip', 'AutoStack', 'Normal', 'Beltable', 'Shoots', 'Quiver', 'BodyLoc1', 'BodyLoc2', 'Rare', 'Magic')
    Write-Tsv 'itemtypes.txt' $ih @(, @('Herb', 'herb', 'misc', '', '0', '0', '0', '0', '0', '0', '1', '1', '', '', '', '', '0', '0'))
    $sh = @('Stat', '*ID', 'Signed', 'Send Bits', 'Save Bits', 'Save Add', 'descfunc', 'descstrpos', 'descstrneg')
    $stats = @(0..367 | ForEach-Object { if ($_ -eq 76) { , @('item_maxhp_percent', '76', '', '9', '9', '', '', '', '') } else { , @("vanilla_stat_$_", "$_", '', '9', '9', '', '', '', '') } })
    Write-Tsv 'itemstatcost.txt' $sh $stats
    $ph = @('code', '*Id', 'stat1', '*Tooltip', '*Notes')
    $props = @(0..292 | ForEach-Object { if ($_ -eq 5) { , @('str', '5', 'strength', 'Str', '') } else { , @("vanilla_prop_$_", "$_", 'strength', '', '') } })
    Write-Tsv 'properties.txt' $ph $props

    $log1 = Run-Installer
    Check ($LASTEXITCODE -eq 0) 'first run succeeds'
    if ($LASTEXITCODE -ne 0) { Write-Host $log1 }
    $names = @('cartographer_map_level', 'cartographer_mon_life', 'cartographer_mon_damage', 'cartographer_mon_fire_res', 'cartographer_mon_cold_res', 'cartographer_mon_light_res', 'cartographer_mon_poison_res', 'cartographer_mon_speed', 'cartographer_map_density')
    $rows = Read-Rows 'itemstatcost.txt'
    Check ($rows.Count -eq 368 + $names.Count) "itemstatcost.txt has $($names.Count) new rows after one run (got $($rows.Count - 368))"
    for ($i = 0; $i -lt $names.Count; $i++) {
        $r = if (368 + $i -lt $rows.Count) { $rows[368 + $i] } else { @('', '') }
        Check ($r[0] -eq $names[$i] -and $r[1] -eq "$(368 + $i)") "stat $($names[$i]) has id $(368 + $i) (got '$($r[0])' id '$($r[1])')"
    }
    $ids = @($rows | ForEach-Object { $_[1] })
    Check (($ids | Select-Object -Unique).Count -eq $ids.Count) 'itemstatcost.txt ids are unique'
    $pnames = @('map-level', 'mon-life', 'mon-damage', 'mon-fire-res', 'mon-cold-res', 'mon-light-res', 'mon-poison-res', 'mon-speed', 'map-density')
    $prows = Read-Rows 'properties.txt'
    Check ($prows.Count -eq 293 + $pnames.Count) "properties.txt has $($pnames.Count) new rows after one run (got $($prows.Count - 293))"
    for ($i = 0; $i -lt $pnames.Count; $i++) {
        $r = if (293 + $i -lt $prows.Count) { $prows[293 + $i] } else { @('', '', '') }
        Check ($r[0] -eq $pnames[$i] -and $r[1] -eq "$(293 + $i)" -and $r[2] -eq $names[$i]) "property $($pnames[$i]) has id $(293 + $i) and stat $($names[$i]) (got '$($r[0])' id '$($r[1])' stat '$($r[2])')"
    }
    $tables = 'itemstatcost.txt', 'properties.txt', 'levels.txt', 'lvlmaze.txt', 'itemtypes.txt'
    $before = $tables | ForEach-Object { (Get-FileHash (Join-Path $dir $_)).Hash }
    $log2 = Run-Installer
    Check ($LASTEXITCODE -eq 0) 'second run succeeds'
    $after = $tables | ForEach-Object { (Get-FileHash (Join-Path $dir $_)).Hash }
    Check (($before -join ',') -eq ($after -join ',')) 'a second run changes no table'
} finally {
    Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction SilentlyContinue
}
if ($failed -gt 0) { Write-Host "$failed check(s) failed."; exit 1 }
Write-Host 'All install stat checks passed.'
