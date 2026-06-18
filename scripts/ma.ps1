#Requires -Version 7
<#
.SYNOPSIS
Build a single dcc CP/M test application.

.DESCRIPTION
Compatibility wrapper around scripts/runall.py --app. The build implementation
lives in the Python runner so runall.ps1 and ma.ps1 do not depend on each other.
#>

param(
    [Parameter(Position = 0)]
    [string]$Name,

    [Parameter(Position = 1)]
    [ValidateSet("peep", "nopeep", "opt", "optimized", "o", "noopt", "unopt", "u", "1", "0", "yes", "no", "true", "false")]
    [string]$Mode = "peep",

    [string]$BuildDir = "build",
    [string]$Emulator = "ntvcm"
)

$ErrorActionPreference = "Stop"

if (-not $Name) {
    Write-Error "Name is required. Usage: ma.ps1 <name> [peep|nopeep]"
    exit 1
}

$python = Get-Command python3 -ErrorAction SilentlyContinue
if (-not $python) {
    $python = Get-Command python -ErrorAction SilentlyContinue
}
if (-not $python) {
    Write-Error "Python 3 is required to run scripts/runall.py"
    exit 1
}

$modeLower = $Mode.ToLowerInvariant()
$pythonMode = if (@("peep", "opt", "optimized", "o", "1", "yes", "true") -contains $modeLower) {
    "peep"
} else {
    "nopeep"
}

$runner = Join-Path $PSScriptRoot "runall.py"
& $python.Source $runner --app $Name --mode $pythonMode --build-dir $BuildDir --emulator $Emulator
exit $LASTEXITCODE
