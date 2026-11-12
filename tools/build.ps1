# Builds one or all plugins against the shared PluginSDK (each stays independently buildable; this drives them uniformly).
#   tools\build.ps1                          # all plugins
#   tools\build.ps1 -Plugin better-auras
#   tools\build.ps1 -Test                    # also run each plugin's host unit tests (ctest / *_tests.exe)
# Output: build\<plugin>\Release\*.dll, collected into dist\<plugin>\.
# Plugins differ (CMake in the root or in plugin/, own SDK variable name), so this calls cmake directly and passes
# the shared SDK explicitly instead of going through each plugin's build.ps1.
param(
    [string]$Plugin = 'all',
    [switch]$Test
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$sdk  = Join-Path $repo 'third_party\PluginSDK'
if (-not (Test-Path (Join-Path $sdk 'CMakeLists.txt'))) {
    throw 'PluginSDK missing: run `git submodule update --init --recursive`'
}
function Run([string]$what, [scriptblock]$cmd) { & $cmd; if ($LASTEXITCODE -ne 0) { throw "$what failed ($LASTEXITCODE)" } }

$names = if ($Plugin -eq 'all') { Get-ChildItem (Join-Path $repo 'plugins') -Directory | ForEach-Object Name } else { @($Plugin) }
foreach ($n in $names) {
    $dir = Join-Path $repo "plugins\$n"
    if (-not (Test-Path $dir)) { throw "No such plugin: $n" }
    $src = if (Test-Path (Join-Path $dir 'CMakeLists.txt')) { $dir } elseif (Test-Path (Join-Path $dir 'plugin\CMakeLists.txt')) { Join-Path $dir 'plugin' } else { throw "${n}: no CMakeLists.txt in the plugin root or plugin/" }
    $cmakeText = Get-Content (Join-Path $src 'CMakeLists.txt') -Raw
    $bld = Join-Path $repo "build\$n"
    Write-Host "=== $n"

    $cfg = @('-S', $src, '-B', $bld, '-A', 'x64')
    $sdkVar = [regex]::Match($cmakeText, 'set\((\w*SDK_DIR)\s').Groups[1].Value
    if (-not $sdkVar) { throw "${n}: no *_SDK_DIR variable in its CMakeLists.txt" }
    $cfg += "-D$sdkVar=$sdk"
    $testOpt = [regex]::Match($cmakeText, 'option\((\w+_TESTS)\s').Groups[1].Value
    if ($Test -and $testOpt) { $cfg += "-D$testOpt=ON" }

    Run "$n configure" { cmake @cfg }
    Run "$n build" { cmake --build $bld --config Release }

    $out = Join-Path $repo "dist\$n"
    New-Item -ItemType Directory -Force $out | Out-Null
    $dlls = Get-ChildItem $bld -Recurse -Filter *.dll -ErrorAction SilentlyContinue | Where-Object { $_.Directory.Name -eq 'Release' -and $_.FullName -notmatch '[\\/](PluginSDK|_deps|CMakeFiles)[\\/]' }
    if (-not $dlls) { throw "${n}: no DLL produced" }
    $dlls | Copy-Item -Destination $out -Force
    $dlls | ForEach-Object { Write-Host "  built: $($_.Name)" }
    # A companion folder (d2rl-<id>\, the plugin's own game files such as strings) ships beside the DLL.
    foreach ($c in Get-ChildItem $dir -Directory -Filter 'd2rl-*' -ErrorAction SilentlyContinue) {
        Copy-Item $c.FullName -Destination $out -Recurse -Force
        Write-Host "  companion: $($c.Name)\"
    }

    if ($Test) {
        $ran = $false
        if (Test-Path (Join-Path $bld 'CTestTestfile.cmake')) {
            Run "$n ctest" { ctest --test-dir $bld -C Release --output-on-failure }; $ran = $true
        }
        # Plugins whose tests/ is its own CMake project, not wired into the plugin build.
        $tc = Join-Path $dir 'tests\CMakeLists.txt'
        if (-not $ran -and (Test-Path $tc) -and ((Get-Content $tc -Raw) -match 'cmake_minimum_required') -and ($cmakeText -notmatch 'add_subdirectory\(tests')) {
            $tb = Join-Path $repo "build\$n-tests"
            Run "$n tests configure" { cmake -S (Join-Path $dir 'tests') -B $tb -A x64 }
            Run "$n tests build" { cmake --build $tb --config Release }
            Run "$n ctest" { ctest --test-dir $tb -C Release --output-on-failure }; $ran = $true
        }
        # Option-built test executables without add_test (multispec).
        if (-not $ran) {
            foreach ($e in Get-ChildItem (Join-Path $bld 'Release') -Filter *_tests.exe -ErrorAction SilentlyContinue) {
                Run "$n $($e.Name)" { & $e.FullName }; $ran = $true
            }
        }
        # tests\*.py are d2rAI-toolkit game_checks modules (need the toolkit and a game slot): syntax-check only.
        $py = @('py', 'python') | ForEach-Object { Get-Command $_ -ErrorAction SilentlyContinue } |
            Where-Object { & $_.Source --version *> $null; $LASTEXITCODE -eq 0 } | Select-Object -First 1   # skips the Store shim
        foreach ($f in Get-ChildItem (Join-Path $dir 'tests') -Filter *.py -ErrorAction SilentlyContinue) {
            if ($py) { Run "$n $($f.Name) syntax" { & $py.Source -I -c "import ast,sys; ast.parse(open(sys.argv[1], encoding='utf-8').read())" $f.FullName } }
            Write-Host "  $($f.Name): in-game check module (d2rAI-toolkit game_checks + game slot); syntax-checked only"
            $ran = $true
        }
        if (-not $ran) { Write-Host "  no automated tests for $n" }
    }
}
