# Runs the real game on the graphics card and reports frame times by camera view.
# It opens a window for a few minutes. Build first with tools/build_windows.ps1.
param(
    [ValidateSet("gl_compatibility", "forward_plus", "mobile")]
    [string]$Method = "",
    [string]$Godot = "$HOME\mow-tools\godot\Godot_v4.7.2-stable_win64_console.exe",
    [int]$TimeoutSeconds = 420
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$log = Join-Path $env:TEMP "mow-gpu-benchmark.log"

$env:RTS_AUTO_START_SKIRMISH = "1"
$arguments = @("--path", "`"$root\godot\project`"", "--resolution", "1920x1080", "--script", "res://gpu_benchmark.gd", "--log-file", "`"$log`"")
if ($Method) { $arguments += @("--rendering-method", $Method) }

$process = Start-Process -FilePath $Godot -ArgumentList $arguments -PassThru -NoNewWindow `
    -RedirectStandardOutput "$log.out" -RedirectStandardError "$log.err"
$samples = @()
$deadline = (Get-Date).AddSeconds($TimeoutSeconds)
while (-not $process.HasExited -and (Get-Date) -lt $deadline) {
    $line = nvidia-smi --query-gpu=utilization.gpu,memory.used --format=csv,noheader,nounits
    $parts = $line -split ",\s*"
    $samples += [pscustomobject]@{ Utilization = [int]$parts[0]; MemoryMiB = [int]$parts[1] }
    Start-Sleep -Seconds 1
}
if (-not $process.HasExited) {
    Stop-Process -Id $process.Id -Force
    Write-Output "TIMED OUT after $TimeoutSeconds s"
}

Get-Content "$log.out" | Select-String -Pattern "GPU_BENCHMARK|OpenGL API|Vulkan|D3D12|Direct3D" | ForEach-Object { $_.Line }
Get-Content "$log.out", "$log.err" | Select-String -Pattern "SCRIPT ERROR|^ERROR:" | Select-Object -First 15 | ForEach-Object { $_.Line }
$peak = ($samples | Measure-Object Utilization -Maximum).Maximum
$memory = ($samples | Measure-Object MemoryMiB -Maximum).Maximum
Write-Output "NVIDIA_SMI peak_utilization=$peak% peak_memory=${memory}MiB samples=$($samples.Count)"
