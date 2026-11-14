<#
.SYNOPSIS
    Offline test for stage_check.ps1 (issue #461): a fake slot root, stage twice, restore, then compare SHA-256 before and after.

.DESCRIPTION
    Builds a fake slot root under -Work (a temp folder by default) with its own config (MODKIT_TESTING_CONFIG), a slot
    that already holds its own maps.txt, shapes.txt, cartographer.dll, a plugin and a save, then stages a Cartographer
    check into slot 10 twice (the second run must not back the staged copies up again) and restores it. The slot's
    files must come back byte-identical and nothing new may be left behind. Also checks that slot 5 is refused.
    Never touches a real slot: every path is under -Work, and the game is never started.

.PARAMETER Toolkit
    d2rAI-toolkit checkout (its testing\ folder is used by stage_check.ps1).
.PARAMETER Work
    Scratch root. Removed at the end unless -KeepWork.
#>
[CmdletBinding()]
param(
    [string]$Toolkit = (Join-Path $env:USERPROFILE 'source\repos\d2rAI-toolkit'),
    [string]$Work = (Join-Path ([IO.Path]::GetTempPath()) ('stage-check-test-' + [guid]::NewGuid().ToString('N').Substring(0, 8))),
    [switch]$KeepWork
)

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$stage = Join-Path $here 'stage_check.ps1'
$script:fails = 0
function Check([bool]$ok, [string]$what) { if ($ok) { Write-Host "PASS  $what" } else { Write-Host "FAIL  $what"; $script:fails++ } }
function Put([string]$path, [string]$text) {
    $null = New-Item -ItemType Directory -Force (Split-Path $path -Parent)
    [IO.File]::WriteAllText($path, $text)
}
function Sha([string]$path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
# SHA-256 of every file under the slot and the save folder, keyed by path relative to the fake root.
function Snap {
    $h = [ordered]@{}
    foreach ($dir in @((Join-Path $Work 'slots'), (Join-Path $Work 'sg'))) {
        foreach ($f in Get-ChildItem -LiteralPath $dir -Recurse -File) { $h[$f.FullName.Substring($Work.Length)] = Sha $f.FullName }
    }
    return $h
}

$slot = Join-Path $Work 'slots\Slot10'
$mod = Join-Path $slot 'mods\D2RToolsTest'
$carto = Join-Path $mod 'D2RToolsTest.mpq\data\global\excel\d2rloader\cartographer'
$plugins = Join-Path $mod 'd2rloader\plugins'
$saveDir = Join-Path $Work 'sg\mods\D2RToolsTest-Slot10'
$src = Join-Path $Work 'src'
$cfg = Join-Path $Work 'modkit-testing.toml'
$harness = Join-Path $Work 'no-harness.dll'
$oldEnv = @{ cfg = $env:MODKIT_TESTING_CONFIG; slots = $env:MODKIT_SLOTS_FILE; project = $env:MODKIT_PROJECT }

try {
    # The slot's own state (-Restore must leave every byte of it as it is).
    Put (Join-Path $carto 'maps.txt') "slot own maps`r`n"
    Put (Join-Path $carto 'shapes.txt') "slot own shapes`r`n"
    Put (Join-Path $plugins 'cartographer.dll') 'OLD-DLL'
    Put (Join-Path $plugins 'other.dll') 'OTHER-PLUGIN'
    Put (Join-Path $saveDir 'Own.d2s') 'OWN-SAVE'
    Put (Join-Path $slot 'd2rloader\config\d2rloader.toml') "[d2rloader]`ndefault_mod = `"OtherMod`"`n"
    # The check's inputs.
    Put (Join-Path $src 'tables\maps.txt') "new maps`r`n"
    Put (Join-Path $src 'tables\shapes.txt') "new shapes`r`n"
    Put (Join-Path $src 'tables\bosses.txt') "new bosses`r`n"
    Put (Join-Path $src 'saves\Own.d2s') 'STAGED-SAVE'
    Put (Join-Path $src 'saves\Test.d2s') 'TEST-SAVE'
    Put (Join-Path $src 'cartographer.dll') 'NEW-DLL'
    $fwd = { param($p) $p -replace '\\', '/' }
    $toml = @"
[game]
install_dir = "$(& $fwd (Join-Path $Work 'FakeInstall'))"
slots_dir = "$(& $fwd (Join-Path $Work 'slots'))"
slot_dir_template = "Slot{slot}"
slot_count = 11
copy_mode = "copy"
link_dirs = ["Data"]
exclude = ["Logs"]
mods_policy = "skip"

[mod]
name = "D2RToolsTest"
folder = "D2RToolsTest"
plugins_subdir = "d2rloader/plugins"
plugin_dlls = ["test-harness.dll"]

[saves]
root = "$(& $fwd (Join-Path $Work 'sg\mods'))"
slot1_name = "{folder}"
slot_name = "{folder}-Slot{slot}"

[runs]
dir = "$(& $fwd (Join-Path $Work 'runs'))"
"@
    [IO.File]::WriteAllText($cfg, $toml)
    $env:MODKIT_TESTING_CONFIG = $cfg
    # The slot table: a copy of the owners table in the scratch root, so the user's own ~/.d2r-modkit/slots.toml is not read.
    $slotsFile = Join-Path $Work 'slots.toml'
    [IO.File]::WriteAllText($slotsFile, "[owners]`n`"10-11`" = `"d2r-tools-test`"`n")
    $env:MODKIT_SLOTS_FILE = $slotsFile
    $env:MODKIT_PROJECT = 'd2r-tools-test'

    $before = Snap
    $stageArgs = @{
        Slot = 10; Owner = 't461'; Dll = (Join-Path $src 'cartographer.dll'); Tables = (Join-Path $src 'tables')
        Saves = (Join-Path $src 'saves'); Toolkit = $Toolkit; HarnessDll = $harness
    }

    Write-Host '--- stage 1 (slot 10) ---'
    & $stage @stageArgs | Out-Host
    $backup = Join-Path $mod 'stage_check-backup'
    Check ((Sha (Join-Path $carto 'maps.txt')) -eq (Sha (Join-Path $src 'tables\maps.txt'))) 'stage: slot maps.txt replaced by the check table'
    Check ((Sha (Join-Path $saveDir 'Own.d2s')) -eq (Sha (Join-Path $src 'saves\Own.d2s'))) 'stage: slot save replaced by the test save'
    # Set aside: maps.txt, shapes.txt, cartographer.dll and Own.d2s (the toml is edited by line, other.dll is parked elsewhere).
    Check (@(Get-ChildItem -LiteralPath $backup -File -ErrorAction SilentlyContinue).Count -eq 4) 'stage: 4 overwritten slot files set aside in stage_check-backup'
    Check (Test-Path (Join-Path $Work 'runs\game.lock.10')) 'stage: slot 10 locked by the owner'

    Write-Host '--- stage 2 (same owner, same inputs) ---'
    & $stage @stageArgs | Out-Host
    Check (@(Get-ChildItem -LiteralPath $backup -File -ErrorAction SilentlyContinue).Count -eq 4) 'stage 2: nothing set aside again (our own copies are not backed up)'

    Write-Host '--- restore ---'
    & $stage -Slot 10 -Owner t461 -Restore -Toolkit $Toolkit | Out-Host

    $after = Snap
    $diff = @($before.Keys | Where-Object { $after[$_] -ne $before[$_] }) + @($after.Keys | Where-Object { -not $before.Contains($_) })
    foreach ($k in $diff) { Write-Host "  differs: $k (before: $($before[$k]) after: $($after[$k]))" }
    Check ($diff.Count -eq 0) "restore: every file under the slot and save folder matches its SHA-256 from before staging ($($before.Count) files)"
    Check (-not (Test-Path (Join-Path $mod 'stage_check.json'))) 'restore: manifest removed'
    Check (-not (Test-Path (Join-Path $Work 'runs\game.lock.10'))) 'restore: slot 10 unlocked'

    Write-Host '--- slot 5 ---'
    $msg = ''
    try { & $stage -Slot 5 -Owner t461 -Dll (Join-Path $src 'cartographer.dll') -Toolkit $Toolkit *> $null } catch { $msg = $_.Exception.Message }
    Check ($msg -match 'Slot 5 is refused') "slot 5 refused: $msg"
    Check (-not (Test-Path (Join-Path $Work 'runs\game.lock.5'))) 'slot 5: nothing locked'
}
finally {
    $env:MODKIT_TESTING_CONFIG = $oldEnv.cfg; $env:MODKIT_SLOTS_FILE = $oldEnv.slots; $env:MODKIT_PROJECT = $oldEnv.project
    if (-not $KeepWork) { Remove-Item -LiteralPath $Work -Recurse -Force -ErrorAction SilentlyContinue }
}
if ($script:fails -gt 0) { Write-Host "$($script:fails) check(s) FAILED"; exit 1 }
Write-Host 'All checks passed.'
