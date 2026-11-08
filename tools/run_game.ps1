# Starts the game on this PC's graphics card. Build first with tools/build_windows.ps1.
param(
    [string]$Godot = "$HOME\mow-tools\godot\Godot_v4.7.2-stable_win64.exe"
)
$root = Split-Path -Parent $PSScriptRoot
$library = Join-Path $root "godot\project\bin\rts_gdextension.dll"
if (-not (Test-Path $library)) { throw "Game library not built. Run tools/build_windows.ps1 first." }
if (-not (Test-Path $Godot)) { throw "Godot not found at $Godot. Pass its location with -Godot." }
& $Godot --path "$root\godot\project"
