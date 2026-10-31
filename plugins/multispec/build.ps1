# Configures and builds MultiSpec (Visual Studio generator, x64, Release) and runs the unit tests.
#   powershell -ExecutionPolicy Bypass -File build.ps1 [-Mod <mod folder>] [-Sdk <PluginSDK dir>] [-Build <dir>] [-NoTests]
# Output: <Build>\Release\multispec.dll. Copy it to <game>\d2rloader\plugins\ (global plugin; or use install.ps1).
param(
    [string]$Mod = "",       # MULTISPEC_MOD_NAME: mods\<Mod> the plugin ships in ("" = no mod check)
    [string]$Sdk = "",       # PluginSDK directory (default ..\d2rAI-toolkit\third_party\PluginSDK)
    [string]$Build = "",     # build directory (default .\build)
    [switch]$NoTests
)
$ErrorActionPreference = 'Stop'
$Source = $PSScriptRoot
if (-not $Build) { $Build = Join-Path $Source "build" }
$configure = @("-S", $Source, "-B", $Build, "-A", "x64", "-DMULTISPEC_MOD_NAME=$Mod")
if ($Sdk) { $configure += "-DMULTISPEC_SDK_DIR=$((Resolve-Path $Sdk).Path)" }
if (-not $NoTests) { $configure += "-DMULTISPEC_TESTS=ON" }
cmake @configure
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed ($LASTEXITCODE)" }
cmake --build $Build --config Release
if ($LASTEXITCODE -ne 0) { throw "cmake build failed ($LASTEXITCODE)" }
$dll = Join-Path $Build "Release\multispec.dll"
if (-not (Test-Path $dll)) { throw "build finished but $dll is missing" }
if (-not $NoTests) {
    & (Join-Path $Build "Release\multispec_tests.exe")
    if ($LASTEXITCODE -ne 0) { throw "unit tests failed" }
}
Write-Output "built: $dll"
