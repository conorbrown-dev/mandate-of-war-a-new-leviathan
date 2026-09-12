$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
python "$ScriptDir/validate.py" @args
exit $LASTEXITCODE
