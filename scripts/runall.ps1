#Requires -Version 7
<#
.SYNOPSIS
Compatibility wrapper for the Python dcc regression runner.

.DESCRIPTION
The fast implementation lives in scripts/runall.py. This wrapper preserves the
PowerShell command line used by existing docs and habits while keeping the suite
independent of ma.ps1.
#>

param(
    [string]$Emulator = "ntvcm",
    [switch]$NoStackCheck,
    [string]$BuildDir = "build",
    [string]$BaselineDir = "tests/baselines",
    [ValidateSet("peep", "nopeep", "both")]
    [string]$Mode = "both",
    [int]$RunTimeout = 60,
    [switch]$Serial,
    [int]$ThrottleLimit = [Environment]::ProcessorCount
)

$ErrorActionPreference = "Stop"

$python = Get-Command python3 -ErrorAction SilentlyContinue
if (-not $python) {
    $python = Get-Command python -ErrorAction SilentlyContinue
}
if (-not $python) {
    Write-Error "Python 3 is required to run scripts/runall.py"
    exit 1
}

$runner = Join-Path $PSScriptRoot "runall.py"
$argsList = @(
    $runner,
    "--emulator", $Emulator,
    "--build-dir", $BuildDir,
    "--baseline-dir", $BaselineDir,
    "--mode", $Mode,
    "--run-timeout", [string]$RunTimeout,
    "--throttle-limit", [string]$ThrottleLimit
)
if ($NoStackCheck) { $argsList += "--no-stack-check" }
if ($Serial) { $argsList += "--serial" }

& $python.Source @argsList
exit $LASTEXITCODE
