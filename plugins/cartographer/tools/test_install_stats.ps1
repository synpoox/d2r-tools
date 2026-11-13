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
function Run-Installer { $ErrorActionPreference = 'Continue'; & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $installer -ExcelDir $dir 2>&1 | Out-String }
try {
    $lh = @('Name', 'Id', 'Act', 'DrlgType', 'LevelType', 'Depend', 'OffsetX', 'OffsetY', 'Position', 'SizeX', 'SizeY', 'Quest', 'LevelGroup', 'LevelName', 'LevelEntry', 'LevelWarp', 'SubType', 'SubTheme', 'Waypoint') +
        (0..7 | ForEach-Object { "Vis$_" }) + (0..7 | ForEach-Object { "Warp$_" })
    function Level($name, $id, $act, $drlg, $type) {
        $c = @{ Name = $name; Id = "$id"; Act = "$act"; DrlgType = "$drlg"; LevelType = "$type"; Depend = '0'; OffsetX = '0'; OffsetY = '0'; Position = '0'; SizeX = '48'; SizeY = '48'; Quest = '0'; LevelGroup = 'Null'; LevelName = $name; LevelEntry = ''; LevelWarp = ''; SubType = '-1'; SubTheme = '-1'; Waypoint = '255' }
        , @($lh | ForEach-Object { if ($c.ContainsKey($_)) { $c[$_] } elseif ($_ -like 'Vis*') { '0' } elseif ($_ -like 'Warp*') { '-1' } else { '' } })
    }
    Write-Tsv 'levels.txt' $lh @((Level 'Cave 1' 8 0 1 3), (Level 'Crypt 1' 9 0 1 4), (Level 'River of Flame' 107 3 1 28), (Level 'Bloody Foothills' 110 4 3 30), (Level 'Barricade 1' 111 4 3 21), (Level 'Frozen Tundra' 117 4 3 31), (Level 'Icy Cellar' 119 4 1 33),(Level 'Last' 136 4 3 21))
    Write-Tsv 'lvlmaze.txt' @('Name', 'Level', 'Rooms', 'SizeX', 'SizeY', 'Merge') @(, @('Cave 1', '8', '3', '10', '10', '0'))
    $ih = @('ItemType', 'Code', 'Equiv1', 'Equiv2', 'Repair', 'Body', 'Throwable', 'Reload', 'ReEquip', 'AutoStack', 'Normal', 'Beltable', 'Shoots', 'Quiver', 'BodyLoc1', 'BodyLoc2', 'Rare', 'Magic')
    Write-Tsv 'itemtypes.txt' $ih @(, @('Herb', 'herb', 'misc', '', '0', '0', '0', '0', '0', '0', '1', '1', '', '', '', '', '0', '0'))
    $sh = @('Stat', '*ID', 'Signed', 'Send Bits', 'Save Bits', 'Save Add', 'descfunc', 'descstrpos', 'descstrneg', '1.09-Save Bits', '1.09-Save Add')
    $stats = @(0..367 | ForEach-Object { if ($_ -eq 76) { , @('item_maxhp_percent', '76', '', '9', '9', '', '', '', '', '9', '') } else { , @("vanilla_stat_$_", "$_", '', '9', '9', '', '', '', '', '9', '') } })
    Write-Tsv 'itemstatcost.txt' $sh $stats
    $ph = @('code', '*Id', 'stat1', '*Tooltip', '*Notes')
    $props = @(0..292 | ForEach-Object { if ($_ -eq 5) { , @('str', '5', 'strength', 'Str', '') } else { , @("vanilla_prop_$_", "$_", 'strength', '', '') } })
    Write-Tsv 'properties.txt' $ph $props

    $log1 = Run-Installer
    Check ($LASTEXITCODE -eq 0) 'first run succeeds'
    $firstTypeRows = @(Read-Rows 'itemtypes.txt' | Where-Object { $_[1] -eq 'cmap' })
    Check ($firstTypeRows.Count -eq 1) 'a fresh run writes one cmap item type row'
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

    $density = $rows | Where-Object { $_[0] -eq 'cartographer_map_density' }
    Check ($density[3] -eq '20' -and $density[4] -eq '20' -and $density[5] -eq '524288' -and $density[9] -eq '20' -and $density[10] -eq '524288') "the density stat is 20 send bits, 20 save bits, save add 524288 (got $($density[3]) $($density[4]) $($density[5]))"
    $monLife = $rows | Where-Object { $_[0] -eq 'cartographer_mon_life' }
    Check ($monLife[3] -eq '11' -and $monLife[4] -eq '10' -and $monLife[5] -eq '512' -and $monLife[9] -eq '10' -and $monLife[10] -eq '512') 'a monster stat keeps 11 / 10 / 512'

    # A density row an earlier run wrote with the monster widths is widened in place (same id), and then a run changes nothing.
    $statPath = Join-Path $dir 'itemstatcost.txt'
    $text = [System.IO.File]::ReadAllText($statPath)
    $old = ($density -join "`t")
    $narrow = $density.Clone(); $narrow[3] = '11'; $narrow[4] = '10'; $narrow[5] = '512'; $narrow[9] = '10'; $narrow[10] = '512'
    Check ($text.Contains($old)) 'the density row is found in the table'
    [System.IO.File]::WriteAllText($statPath, $text.Replace($old, ($narrow -join "`t")), (New-Object System.Text.UTF8Encoding($false)))
    $log3 = Run-Installer
    Check ($LASTEXITCODE -eq 0) 'a run on an old-width density row succeeds'
    $rows3 = Read-Rows 'itemstatcost.txt'
    $fixed = $rows3 | Where-Object { $_[0] -eq 'cartographer_map_density' }
    Check ($rows3.Count -eq $rows.Count) 'the repair adds no row'
    Check ($fixed[1] -eq $density[1] -and $fixed[3] -eq '20' -and $fixed[4] -eq '20' -and $fixed[5] -eq '524288' -and $fixed[9] -eq '20' -and $fixed[10] -eq '524288') "the old-width density row is widened and keeps its id (got id $($fixed[1]), $($fixed[3]) $($fixed[4]) $($fixed[5]))"
    $before = (Get-FileHash $statPath).Hash
    $log4 = Run-Installer
    Check ((Get-FileHash $statPath).Hash -eq $before) 'a run after the repair changes nothing'

    # Only that one old row is repaired: a monster stat and a density row with other widths, set by hand, stay as they are.
    $text = [System.IO.File]::ReadAllText($statPath)
    $monOld = ($monLife -join "`t"); $monHand = $monLife.Clone(); $monHand[3] = '12'; $monHand[4] = '11'; $monHand[5] = '1024'
    $denOld = ($fixed -join "`t"); $denHand = $fixed.Clone(); $denHand[3] = '15'; $denHand[4] = '15'; $denHand[5] = '16384'
    Check ($text.Contains($monOld) -and $text.Contains($denOld)) 'the monster and density rows are found in the table'
    [System.IO.File]::WriteAllText($statPath, $text.Replace($monOld, ($monHand -join "`t")).Replace($denOld, ($denHand -join "`t")), (New-Object System.Text.UTF8Encoding($false)))
    $before = (Get-FileHash $statPath).Hash
    $log5 = Run-Installer
    Check ($LASTEXITCODE -eq 0 -and (Get-FileHash $statPath).Hash -eq $before) 'a monster row and a density row with other widths are left alone'

    # The Map item type (cmap): a fresh run writes it with Rare on and Magic and Normal off, a row from an older install (Magic 1, Rare 0) is repaired in place, and a run after that changes nothing.
    function Get-MapType { $h = (Get-Content (Join-Path $dir 'itemtypes.txt') -TotalCount 1) -split "`t"; $r = @(Read-Rows 'itemtypes.txt' | Where-Object { $_[1] -eq 'cmap' }); @{ Header = $h; Row = $r } }
    $mt = Get-MapType
    $iRare = [array]::IndexOf($mt.Header, 'Rare'); $iMagic = [array]::IndexOf($mt.Header, 'Magic'); $iNormal = [array]::IndexOf($mt.Header, 'Normal')
    Check ($iRare -ge 0 -and $iMagic -ge 0 -and $iNormal -ge 0) 'itemtypes.txt has the Rare, Magic and Normal columns'
    if ($iRare -lt 0 -or $iMagic -lt 0 -or $iNormal -lt 0) { throw 'itemtypes.txt lacks a Rare, Magic or Normal column' }
    Check ($mt.Row.Count -eq 1) 'the cmap item type row is still one row'
    $cmap = $mt.Row[0]
    Check ($cmap[$iRare] -eq '1' -and $cmap[$iMagic] -eq '0' -and $cmap[$iNormal] -eq '0') "the new cmap row has Rare 1, Magic 0, Normal 0 (got $($cmap[$iRare]) $($cmap[$iMagic]) $($cmap[$iNormal]))"
    $typePath = Join-Path $dir 'itemtypes.txt'
    $typeText = [System.IO.File]::ReadAllText($typePath)
    $oldRow = ($cmap -join "`t"); $staleRow = $cmap.Clone(); $staleRow[$iRare] = '0'; $staleRow[$iMagic] = '1'; $staleRow[$iNormal] = ''
    Check ($typeText.Contains($oldRow)) 'the cmap row is found in the table'
    [System.IO.File]::WriteAllText($typePath, $typeText.Replace($oldRow, ($staleRow -join "`t")), (New-Object System.Text.UTF8Encoding($false)))
    $log6 = Run-Installer
    Check ($LASTEXITCODE -eq 0) 'a run on an old cmap row succeeds'
    $mt = Get-MapType
    Check ($mt.Row.Count -eq 1) 'the repair adds no item type row'
    $fixedType = $mt.Row[0]
    Check ($fixedType[$iRare] -eq '1' -and $fixedType[$iMagic] -eq '0' -and $fixedType[$iNormal] -eq '0') "the old cmap row is repaired to Rare 1, Magic 0, Normal 0 (got $($fixedType[$iRare]) $($fixedType[$iMagic]) $($fixedType[$iNormal]))"
    Check (($fixedType -join "`t") -eq $oldRow) 'the repair changes only those cells'
    $before = (Get-FileHash $typePath).Hash
    $log7 = Run-Installer
    Check ($LASTEXITCODE -eq 0 -and (Get-FileHash $typePath).Hash -eq $before) 'a second run after the cmap repair changes nothing'

    # A table with no room for a new stat id (511 rows: an item save cannot hold id 511) stops the run, and nothing is written, not even the
    # earlier itemtypes.txt repair (every table is written at the end). A scratch copy of the installed folder, with the stat rows replaced and
    # the cmap row made stale, so the run would have changed itemtypes.txt before it reached the stat table.
    $full = $dir
    $dir = $dir + '-full'
    New-Item -ItemType Directory -Path $dir | Out-Null
    Get-ChildItem -LiteralPath $full -File | Where-Object { $_.Name -notlike '*.bak' } | Copy-Item -Destination $dir
    $fullStats = @(0..510 | ForEach-Object { if ($_ -eq 76) { , @('item_maxhp_percent', '76', '', '9', '9', '', '', '', '', '9', '') } else { , @("vanilla_stat_$_", "$_", '', '9', '9', '', '', '', '', '9', '') } })
    Write-Tsv 'itemstatcost.txt' $sh $fullStats
    $fullTypePath = Join-Path $dir 'itemtypes.txt'
    $fullText = [System.IO.File]::ReadAllText($fullTypePath)
    Check ($fullText.Contains($oldRow)) 'the full-table folder has the cmap row'
    [System.IO.File]::WriteAllText($fullTypePath, $fullText.Replace($oldRow, ($staleRow -join "`t")), (New-Object System.Text.UTF8Encoding($false)))
    $beforeFull = (Get-ChildItem -LiteralPath $dir -File | Sort-Object Name | ForEach-Object { "$($_.Name)=$((Get-FileHash $_.FullName).Hash)" }) -join ','
    $logFull = Run-Installer
    Check ($LASTEXITCODE -ne 0) 'a run on a stat table with 511 rows fails'
    Check ($logFull.Contains('511')) 'the failure names the 511 limit'
    $afterFull = (Get-ChildItem -LiteralPath $dir -File | Sort-Object Name | ForEach-Object { "$($_.Name)=$((Get-FileHash $_.FullName).Hash)" }) -join ','
    Check ($beforeFull -eq $afterFull) 'the failed run wrote no table and made no backup'
    Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction SilentlyContinue
    $dir = $full
} finally {
    Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction SilentlyContinue
}
if ($failed -gt 0) { Write-Host "$failed check(s) failed."; exit 1 }
Write-Host 'All install stat checks passed.'
