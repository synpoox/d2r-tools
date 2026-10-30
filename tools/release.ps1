# Builds a suite release: one zip per plugin listed in release\suite.json, a bundle of all of them, SHA256SUMS.txt and
# the release notes. The notes are written for players: plugin table, changes (from each plugin's dated CHANGELOG
# section), requirements, installation, validation and limits, checksums.
#   tools\release.ps1                      # build everything into dist\release\<suite>\ (nothing is published)
#   tools\release.ps1 -NoBuild             # reuse dist\<plugin> from an earlier tools\build.ps1 run
# Returns {Tag, Title, Notes, Assets}; .github/workflows/release.yml publishes them with `gh release create`.
# Every plugin tag (<plugin>/v<version>) named in the manifest must already exist on origin.
param(
    [string]$Suite = '',        # suite tag, default release-YYYY.MM.DD.N (UTC date, N = next free number on origin)
    [string]$Manifest = '',
    [switch]$NoBuild
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
if (-not $Manifest) { $Manifest = Join-Path $repo 'release\suite.json' }
$man = [IO.File]::ReadAllText($Manifest) | ConvertFrom-Json

if (-not $Suite) {
    $day = (Get-Date).ToUniversalTime().ToString('yyyy.MM.dd')
    $existing = @(git -C $repo ls-remote --tags origin "refs/tags/release-$day.*") | Where-Object { $_ }
    $Suite = "release-$day.$($existing.Count + 1)"
}
if ($Suite -notmatch '^release-(\d{4}\.\d{2}\.\d{2}\.\d+)$') { throw "Suite tag must look like release-YYYY.MM.DD.N: $Suite" }
$title = "D2R Tools $($Matches[1])"
$out = Join-Path $repo "dist\release\$Suite"
if (Test-Path $out) { Remove-Item $out -Recurse -Force }
New-Item -ItemType Directory -Force $out | Out-Null
$root = [IO.File]::ReadAllText((Join-Path $repo 'README.md'))

function Read-Section([string]$text, [string]$version) {   # -> @{ Date; Body } of "## <version> - <date>" in a changelog
    $m = [regex]::Match($text, "(?ms)^##\s+\[?$([regex]::Escape($version))\]?\s*-\s*(\d{4}-\d{2}-\d{2})[^\r\n]*\r?\n(.*?)(?=^##\s|\z)")
    if (-not $m.Success) { throw "CHANGELOG has no '## $version - YYYY-MM-DD' section" }
    @{ Date = $m.Groups[1].Value; Body = $m.Groups[2].Value.Trim() }
}

$rows = @(); $changes = @(); $valid = @(); $files = @()
$bundle = Join-Path $out '_bundle'
foreach ($p in $man.plugins.PSObject.Properties) {
    $name = $p.Name; $ver = $p.Value.version; $dir = Join-Path $repo "plugins\$name"
    if (-not (Test-Path $dir)) { throw "No such plugin: $name" }
    git -C $repo ls-remote --exit-code --tags origin "refs/tags/$name/v$ver" *> $null
    if ($LASTEXITCODE -ne 0) { throw "Tag $name/v$ver does not exist on origin; tag the plugin release first (docs/RELEASING.md)" }
    $readme = [IO.File]::ReadAllText((Join-Path $dir 'README.md'))
    $display = ([regex]::Match($readme, '(?m)^#\s+(.+?)\s*$')).Groups[1].Value
    if (-not $display) { $display = $name }
    $desc = ([regex]::Match($root, "(?m)^\|\s*\[$([regex]::Escape($name))\]\([^)]*\)\s*\|[^|]*\|\s*(.+?)\s*\|\s*$")).Groups[1].Value
    if (-not $desc) { throw "Root README plugin table has no row for $name" }
    $sec = Read-Section ([IO.File]::ReadAllText((Join-Path $dir 'CHANGELOG.md'))) $ver

    if (-not $NoBuild) { & (Join-Path $PSScriptRoot 'build.ps1') -Plugin $name }
    $dist = Join-Path $repo "dist\$name"
    $dlls = @(Get-ChildItem $dist -Filter *.dll -ErrorAction SilentlyContinue)
    if (-not $dlls) { throw "$name`: no DLL in dist\$name (run tools\build.ps1)" }
    $stage = Join-Path $out "_stage\$name-$ver"
    New-Item -ItemType Directory -Force $stage | Out-Null
    $dlls | Copy-Item -Destination $stage
    Copy-Item (Join-Path $dir 'README.md'), (Join-Path $dir 'CHANGELOG.md') -Destination $stage
    $zip = Join-Path $out "$name-$ver.zip"
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip
    New-Item -ItemType Directory -Force (Join-Path $bundle $name) | Out-Null
    Copy-Item (Join-Path $stage '*') -Destination (Join-Path $bundle $name)
    $files += $zip

    $rows    += "| [$display](https://github.com/synpoox/d2r-tools/tree/main/plugins/$name) | $ver | $($sec.Date) | $desc |"
    $changes += "### $display $ver`n`n$($sec.Body)"
    $v = @($p.Value.validation | ForEach-Object { "- $_" }) + @($p.Value.limits | ForEach-Object { "- Limit: $_" })
    $valid   += "**$display**`n`n$($v -join "`n")"
}
$all = Join-Path $out "d2r-tools-$($Suite -replace '^release-','').zip"
Compress-Archive -Path (Join-Path $bundle '*') -DestinationPath $all
$files += $all
Remove-Item $bundle, (Join-Path $out '_stage') -Recurse -Force

$sums = $files | ForEach-Object { $h = (Get-FileHash $_ -Algorithm SHA256).Hash.ToLower(); [pscustomobject]@{ File = Split-Path $_ -Leaf; Hash = $h; KB = [math]::Round((Get-Item $_).Length / 1KB) } }
$sumFile = Join-Path $out 'SHA256SUMS.txt'
[IO.File]::WriteAllText($sumFile, (($sums | ForEach-Object { "$($_.Hash)  $($_.File)" }) -join "`n") + "`n", (New-Object Text.UTF8Encoding $false))
$sumRows = $sums | ForEach-Object { "| $($_.File) | $($_.KB) KB | ``$($_.Hash)`` |" }

$notes = @"
Plugins for Diablo II: Resurrected on D2RLoader. Download the plugins you want, or everything in one bundle.

## Plugins

| Plugin | Version | Released | What it does |
|---|---|---|---|
$($rows -join "`n")

## Changes

$($changes -join "`n`n")

## Requirements

- Diablo II: Resurrected $($man.game.d2r)
- D2RLoader $($man.game.d2rloader)

Each plugin's README states which game builds it supports.

## Installation

1. Close the game.
2. Download a plugin zip below (or the bundle, which has one folder per plugin) and extract the DLL.
3. Copy the DLL to ``<game>\d2rloader\plugins\`` (works with every mod), or to ``<game>\mods\<Mod>\d2rloader\plugins\``
   for one mod only. Keep one copy of each plugin; never both locations.
4. Start the game through D2RLoader. Plugins with settings write their default config to ``<game>\d2rloader\config\`` on
   first start.

To update, close the game and replace the DLL; your config is kept. Each plugin's README covers its controls and
whether every player in a multiplayer game needs the same version.

## Validation and limits

$($valid -join "`n`n")

## Downloads and checksums

Verify with ``SHA256SUMS.txt`` (``Get-FileHash <file> -Algorithm SHA256`` on Windows).

| File | Size | SHA-256 |
|---|---|---|
$($sumRows -join "`n")

Found a problem? [Open an issue](https://github.com/synpoox/d2r-tools/issues) with the plugin, game and mod versions,
steps to reproduce and your log. Full history: each plugin's ``CHANGELOG.md``.
"@
$notesFile = Join-Path $out 'notes.md'
[IO.File]::WriteAllText($notesFile, $notes, (New-Object Text.UTF8Encoding $false))

Write-Host "Suite $Suite ($title): $($files.Count) zips + checksums in $out"
[pscustomobject]@{ Tag = $Suite; Title = $title; Notes = $notesFile; Assets = @($files) + $sumFile }
