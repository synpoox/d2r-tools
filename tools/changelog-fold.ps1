# Folds a plugin's changelog fragments into its CHANGELOG.md. Each PR adds one file plugins/<name>/changelog.d/<topic>.md
# holding one changelog line, so parallel PRs never conflict on `## Unreleased`.
#   tools\changelog-fold.ps1 -Plugin <name> -Version X.Y.Z [-Date YYYY-MM-DD]   # fragments + any `## Unreleased` lines -> `## X.Y.Z - date`, fragments deleted
#   tools\changelog-fold.ps1 -Check                                              # every fragment is one non-empty line, in a folder named changelog.d, and a .md file (CI)
# Fragments come first (newest first: file name descending, so higher issue numbers on top), then the old Unreleased lines. The file's BOM and line endings are kept.
param(
    [string]$Plugin = '',
    [string]$Version = '',
    [string]$Date = '',
    [string]$Root = '',          # repo root, default this repo (for tests on a scratch copy)
    [switch]$Check
)
$ErrorActionPreference = 'Stop'
if (-not $Root) { $Root = Split-Path $PSScriptRoot -Parent }

function Get-Fragments([string]$dir) {   # -> @{ Name; Line } per fragment, sorted by name descending; throws unless each is one line
    $d = Join-Path $dir 'changelog.d'
    if (-not (Test-Path $d)) { return @() }
    $bad = @(Get-ChildItem $d | Where-Object { $_.PSIsContainer -or $_.Extension -ne '.md' })
    if ($bad) { throw "${d}: only .md fragment files belong here, found $($bad[0].Name)" }
    foreach ($f in (Get-ChildItem $d -File -Filter *.md | Sort-Object Name -Descending)) {
        $lines = @([IO.File]::ReadAllText($f.FullName).TrimStart([char]0xFEFF) -split '\r?\n' | Where-Object { $_.Trim() })
        if ($lines.Count -ne 1) { throw "$($f.FullName): a changelog fragment is exactly one line (found $($lines.Count))" }
        $line = $lines[0].Trim()
        if ($line -notmatch '^[-*]\s') { $line = "- $line" }
        [pscustomobject]@{ Name = $f.Name; Path = $f.FullName; Line = $line }
    }
}

if ($Check) {
    foreach ($p in Get-ChildItem (Join-Path $Root 'plugins') -Directory) {
        $odd = @(Get-ChildItem $p.FullName -Directory | Where-Object { $_.Name -ne 'changelog.d' -and $_.Name -match '^changelogs?(\.d)?$|^changelog' })
        if ($odd) { throw "$($odd[0].FullName): fragments belong in a folder named changelog.d" }
        Get-Fragments $p.FullName | Out-Null
    }
    Write-Host 'Changelog fragments OK'
    return
}

if (-not $Plugin -or $Version -notmatch '^\d+\.\d+\.\d+$') { throw 'Usage: changelog-fold.ps1 -Plugin <name> -Version X.Y.Z [-Date YYYY-MM-DD]  |  -Check' }
if (-not $Date) { $Date = Get-Date -Format 'yyyy-MM-dd' }
if ($Date -notmatch '^\d{4}-\d{2}-\d{2}$') { throw "Date must be YYYY-MM-DD: $Date" }
$dir = Join-Path (Join-Path $Root 'plugins') $Plugin
$file = Join-Path $dir 'CHANGELOG.md'
if (-not (Test-Path $file)) { throw "No changelog: $file" }

$bytes = [IO.File]::ReadAllBytes($file)
$bom = $bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF   # keep the file's BOM as it is
$text = [IO.File]::ReadAllText($file)
$nl = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
if ($text -match "(?m)^##\s+\[?$([regex]::Escape($Version))\]?\s*-") { throw "CHANGELOG already has a section for $Version" }

$frags = @(Get-Fragments $dir)
$m = [regex]::Match($text, '(?ms)^##\s+Unreleased[^\r\n]*\r?\n(.*?)(?=^##\s|\z)')
$old = if ($m.Success) { ($m.Groups[1].Value -split '\r?\n' | Where-Object { $_.Trim() }) } else { @() }
$old = @($old)
if (-not $frags -and -not $old) { throw "$Plugin`: nothing to fold (no fragments, no Unreleased lines)" }

$body = @($frags | ForEach-Object { $_.Line }) + $old
$section = "## $Version - $Date$nl$nl" + ($body -join $nl) + $nl + $nl
if ($m.Success) {
    $text = $text.Substring(0, $m.Index) + $section + $text.Substring($m.Index + $m.Length)
} else {
    $h = [regex]::Match($text, '(?m)^##\s')   # first release heading, or the end when there is none
    $at = if ($h.Success) { $h.Index } else { $text.Length }
    $text = $text.Substring(0, $at) + $section + $text.Substring($at)
}
$text = $text.TrimEnd() + $nl
[IO.File]::WriteAllText($file, $text, (New-Object Text.UTF8Encoding $bom))
foreach ($f in $frags) { Remove-Item $f.Path -Force }
Write-Host "$Plugin $Version`: folded $($frags.Count) fragment(s) and $($old.Count) Unreleased line(s) into $file"
