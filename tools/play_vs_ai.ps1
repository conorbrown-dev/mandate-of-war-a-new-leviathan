# Builds the game if needed and starts a skirmish against the AI.
# Double-click "Play vs AI.cmd" in the repository root, or run:
#   tools\play_vs_ai.ps1                         (asks which AI to fight)
#   tools\play_vs_ai.ps1 -Style swarm -Difficulty hard
#   tools\play_vs_ai.ps1 -Menu                   (main menu instead of straight into a match)
# Styles and difficulties are defined in data/ai_doctrines.json.
param(
    [ValidateSet("", "balanced", "swarm", "turtle", "tech")]
    [string]$Style = "",
    [ValidateSet("", "easy", "normal", "hard")]
    [string]$Difficulty = "",
    [switch]$Menu,
    [switch]$SkipBuild,
    [string]$Godot = "$HOME\mow-tools\godot\Godot_v4.7.2-stable_win64.exe"
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$project = Join-Path $root "godot\project"
$library = Join-Path $project "bin\rts_gdextension.dll"

if (-not (Test-Path $Godot)) {
    throw "Godot 4.7.2 not found at $Godot. Install it there, or pass its location with -Godot."
}

# Pick the opponent from data/ai_doctrines.json, so the menu stays in step with it.
$doctrines = Get-Content (Join-Path $root "data\ai_doctrines.json") -Raw | ConvertFrom-Json
function Choose($title, $options, $default) {
    Write-Host ""
    Write-Host $title -ForegroundColor Cyan
    for ($i = 0; $i -lt $options.Count; $i++) {
        $marker = if ($options[$i].key -eq $default) { " (default)" } else { "" }
        Write-Host ("  {0}. {1}{2} - {3}" -f ($i + 1), $options[$i].name, $marker, $options[$i].description)
    }
    $answer = Read-Host "Choose 1-$($options.Count), or press Enter for the default"
    if ($answer -match '^\d+$' -and [int]$answer -ge 1 -and [int]$answer -le $options.Count) {
        return $options[[int]$answer - 1].key
    }
    return $default
}
if (-not $Menu) {
    if (-not $Style) { $Style = Choose "AI style" $doctrines.doctrines $doctrines.default_doctrine }
    if (-not $Difficulty) { $Difficulty = Choose "AI difficulty" $doctrines.difficulties $doctrines.default_difficulty }
}

# Bring the native game up to date. Nothing is rebuilt when nothing changed.
if (-not $SkipBuild) {
    Write-Host ""
    Write-Host "Building the game (only what changed)..." -ForegroundColor Cyan
    & (Join-Path $PSScriptRoot "build_windows.ps1")
    if ($LASTEXITCODE -ne 0) {
        if (Test-Path $library) {
            Write-Warning "Build failed; starting the last build that worked."
        } else {
            throw "Build failed and there is no earlier build to start."
        }
    }
}
if (-not (Test-Path $library)) { throw "Game library not built. Run tools\build_windows.ps1 first." }

# A fresh checkout needs Godot's one-off asset import before the first start.
if (-not (Test-Path (Join-Path $project ".godot\imported"))) {
    Write-Host "Importing assets for the first run (a few minutes)..." -ForegroundColor Cyan
    $console = $Godot -replace '\.exe$', '_console.exe'
    $importer = if (Test-Path $console) { $console } else { $Godot }
    & $importer --headless --path $project --import | Out-Null
}

if ($Menu) {
    Remove-Item Env:RTS_AUTO_START_SKIRMISH -ErrorAction SilentlyContinue
    Write-Host "Starting the main menu."
} else {
    $env:RTS_AUTO_START_SKIRMISH = "1"
    $env:RTS_AI_DOCTRINE = $Style
    $env:RTS_AI_DIFFICULTY = $Difficulty
    Write-Host ""
    Write-Host "Starting a skirmish against a $Difficulty $Style AI." -ForegroundColor Green
}
Write-Host "Finished matches are saved to $env:APPDATA\Godot\app_userdata\Near-Future RTS\match_records"
Start-Process -FilePath $Godot -ArgumentList @("--path", "`"$project`"")
