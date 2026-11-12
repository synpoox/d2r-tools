# Builds a suite release: one zip per plugin listed in release\suite.json, a bundle of all of them, SHA256SUMS.txt and
# the release notes: each plugin's dated CHANGELOG section, nothing else, so the downloads stay near the top.
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

$changes = @(); $files = @()
$bundle = Join-Path $out '_bundle'
foreach ($p in $man.plugins.PSObject.Properties) {
    $name = $p.Name; $ver = $p.Value.version; $dir = Join-Path $repo "plugins\$name"
    if (-not (Test-Path $dir)) { throw "No such plugin: $name" }
    git -C $repo ls-remote --exit-code --tags origin "refs/tags/$name/v$ver" *> $null
    if ($LASTEXITCODE -ne 0) { throw "Tag $name/v$ver does not exist on origin; tag the plugin release first (docs/RELEASING.md)" }
    $readme = [IO.File]::ReadAllText((Join-Path $dir 'README.md'))
    $display = ([regex]::Match($readme, '(?m)^#\s+(.+?)\s*$')).Groups[1].Value
    if (-not $display) { $display = $name }
    if ($root -notmatch "(?m)^\|\s*\[$([regex]::Escape($name))\]\(") { throw "Root README plugin table has no row for $name" }
    $sec = Read-Section ([IO.File]::ReadAllText((Join-Path $dir 'CHANGELOG.md'))) $ver

    if (-not $NoBuild) { & (Join-Path $PSScriptRoot 'build.ps1') -Plugin $name }
    $dist = Join-Path $repo "dist\$name"
    $dlls = @(Get-ChildItem $dist -Filter *.dll -ErrorAction SilentlyContinue)
    if (-not $dlls) { throw "$name`: no DLL in dist\$name (run tools\build.ps1)" }
    $stage = Join-Path $out "_stage\$name-$ver"
    New-Item -ItemType Directory -Force $stage | Out-Null
    $dlls | Copy-Item -Destination $stage
    Get-ChildItem $dist -Directory -Filter 'd2rl-*' -ErrorAction SilentlyContinue | Copy-Item -Destination $stage -Recurse
    Copy-Item (Join-Path $dir 'README.md'), (Join-Path $dir 'CHANGELOG.md') -Destination $stage
    $zip = Join-Path $out "$name-$ver.zip"
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip
    New-Item -ItemType Directory -Force (Join-Path $bundle $name) | Out-Null
    Copy-Item (Join-Path $stage '*') -Destination (Join-Path $bundle $name)
    $files += $zip

    $changes += "###### $display $ver`n`n$($sec.Body)"
}
$all = Join-Path $out "d2r-tools-$($Suite -replace '^release-','').zip"
Compress-Archive -Path (Join-Path $bundle '*') -DestinationPath $all
$files += $all
Remove-Item $bundle, (Join-Path $out '_stage') -Recurse -Force

$sumFile = Join-Path $out 'SHA256SUMS.txt'
$sums = $files | ForEach-Object { "$((Get-FileHash $_ -Algorithm SHA256).Hash.ToLower())  $(Split-Path $_ -Leaf)" }
[IO.File]::WriteAllText($sumFile, ($sums -join "`n") + "`n", (New-Object Text.UTF8Encoding $false))

$notes = ($changes -join "`n`n") + "`n"
$notesFile = Join-Path $out 'notes.md'
[IO.File]::WriteAllText($notesFile, $notes, (New-Object Text.UTF8Encoding $false))

Write-Host "Suite $Suite ($title): $($files.Count) zips + checksums in $out"
[pscustomobject]@{ Tag = $Suite; Title = $title; Notes = $notesFile; Assets = @($files) + $sumFile }
