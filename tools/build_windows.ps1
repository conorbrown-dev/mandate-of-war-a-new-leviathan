# Builds the native game and test programs for Windows with MSVC.
# Needs Visual Studio or the Build Tools with the C++ workload.
param(
    [string]$BuildDir = "build/windows",
    [switch]$Test
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw "No Visual Studio C++ tools found" }

# vcvars sets the compiler environment, and also puts CMake and Ninja on PATH.
$vcvars = Join-Path $vs "VC\Auxiliary\Build\vcvars64.bat"
$steps = "cmake -S . -B `"$BuildDir`" -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build `"$BuildDir`""
if ($Test) { $steps += " && ctest --test-dir `"$BuildDir`" --output-on-failure" }
cmd /c "`"$vcvars`" >nul && $steps"
exit $LASTEXITCODE
