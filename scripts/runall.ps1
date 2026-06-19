#Requires -Version 7
<#
.SYNOPSIS
Comprehensive test suite: builds and runs all test applications with output
verification against baseline.

.DESCRIPTION
Builds all *.c files in tests/ folder using ma.ps1, executes each under the
emulator, and compares output against per-app baselines in tests/baselines/
(one <app>.txt per test). Comparison is keyed by app name, so test discovery
order does not matter. Uses tests/_test_overrides.json for test-specific arguments and
stack sizes.

Supports per-test arguments (e.g., ttt with "10" as input) and custom stack
size overrides (e.g., cobint needs 1536 bytes, triangle needs 768).

.PARAMETER Emulator
  Emulator command to run .COM files (default: "ntvcm").

.PARAMETER NoStackCheck
  Disable the lightweight stack-overflow guard (-fstack-check). The guard is
  ON by default for all apps; pass -NoStackCheck to build without it.

.PARAMETER BuildDir
  Working directory for build artifacts (default: "build").

.PARAMETER BaselineDir
  Directory of per-app baseline files for verification (default: "tests/baselines").
  Each file is named <app>.txt and holds that app's exact expected stdout.

.PARAMETER Mode
  Which optimization pass(es) to build and verify (default: "both"):
    peep   - optimized (runs the dccpeep peephole optimizer)
    nopeep - unoptimized (skips dccpeep)
    both   - builds and verifies each app twice, once per mode, against the
             same baseline (two builds per app)

.PARAMETER Serial
  Build and verify apps sequentially in the shared build directory. By default
  the suite runs in parallel; use -Serial as a fallback (e.g. for debugging or
  on constrained machines).

.PARAMETER ThrottleLimit
  Max concurrent apps in parallel mode (default: CPU core count).

.EXAMPLE
  pwsh ./scripts/runall.ps1
  pwsh ./scripts/runall.ps1 -NoStackCheck
  pwsh ./scripts/runall.ps1 -Mode nopeep
  pwsh ./scripts/runall.ps1 -Serial
  pwsh ./scripts/runall.ps1 -ThrottleLimit 8

.NOTES
  App overrides are loaded from tests/_test_overrides.json:
    - args: command-line arguments for the app
    - stack_size: C stack reserve override (default 512)
    - ignore: set to true to skip building/running this app

  Exit codes:
    0 = all tests passed
    1 = one or more tests failed
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

# Parallel is the default; -Serial forces the sequential fallback.
$Parallel = -not $Serial

# The lightweight stack-overflow guard (-fstack-check) is ON by default; pass
# -NoStackCheck to build without it.
$StackCheck = -not $NoStackCheck

$ErrorActionPreference = "Stop"

# Save terminal settings so ntvcm's raw-mode console I/O doesn't corrupt them.
$_savedStty = & stty -g 2>$null
if ($null -ne $_savedStty) {
    Register-EngineEvent -SourceIdentifier PowerShell.Exiting -Action { stty $_savedStty 2>$null } | Out-Null
}

# Dot-source the build driver once so Invoke-MaBuild runs in-process. This
# avoids spawning a fresh pwsh per build, which is the dominant cost over a
# full suite (hundreds of builds).
. (Join-Path $PSScriptRoot "ma.ps1")

# Get machine name for reporting (platform-specific)
$machineName = $null

if ($IsMacOS) {
    # macOS local host name (e.g. "MacBook-Pro-3")
    try { $machineName = (scutil --get LocalHostName 2>/dev/null).Trim() } catch { }
} elseif ($IsWindows) {
    # Windows computer name
    $machineName = $env:COMPUTERNAME
} elseif ($IsLinux) {
    # Linux static hostname
    try { $machineName = (hostnamectl --static 2>/dev/null).Trim() } catch { }
}

# Fallbacks for any platform
if (!$machineName) { $machineName = $env:COMPUTERNAME }
if (!$machineName) { $machineName = $env:HOSTNAME }
if (!$machineName) { 
    try { 
        $machineName = (hostname 2>/dev/null).Trim()
    } catch { }
}
if (!$machineName) { $machineName = "unknown" }

# Load app overrides
$appOverridesPath = "tests/_test_overrides.json"
if (-not (Test-Path $appOverridesPath)) {
    Write-Warning "App overrides not found: $appOverridesPath"
    $appOverrides = @{}
} else {
    $config = Get-Content $appOverridesPath | ConvertFrom-Json
    $appOverrides = @{}
    foreach ($app in $config.apps) {
        $appOverrides[$app.name] = @{}
        if ($app.args) { $appOverrides[$app.name]['args'] = $app.args }
        if ($app.stack_size) { $appOverrides[$app.name]['stack_size'] = $app.stack_size }
        if ($app.ignore) { $appOverrides[$app.name]['ignore'] = $app.ignore }
    }
}

# Set environment variables
if ($StackCheck) {
    $env:DCC_FORCE_STACK_CHECK = "1"
    Write-Host "--- stack-check: building every app with -fstack-check (default; use -NoStackCheck to disable) ---" -ForegroundColor Cyan
} else {
    Remove-Item env:DCC_FORCE_STACK_CHECK -ErrorAction SilentlyContinue
    Write-Host "--- stack-check disabled (-NoStackCheck) ---" -ForegroundColor DarkGray
}

# Discover test files
$testFiles = @()
$testDir = "tests"
if (Test-Path $testDir -PathType Container) {
    $testFiles = @(Get-ChildItem -Path (Join-Path $testDir "*.c") -File | ForEach-Object { $_.BaseName } | Sort-Object)
}

if ($testFiles.Count -eq 0) {
    Write-Error "No test files found in $testDir" -ErrorAction Stop
}

Write-Host "Found $($testFiles.Count) test applications" -ForegroundColor Cyan

# Helper functions
function Get-AppArgs {
    param([string]$app)
    if ($appOverrides.ContainsKey($app) -and $appOverrides[$app]['args']) {
        return $appOverrides[$app]['args']
    }
    return ""
}

function Get-StackSize {
    param([string]$app)
    $globalStack = $env:STACK_SIZE
    if ($globalStack) { return $globalStack }
    if ($appOverrides.ContainsKey($app) -and $appOverrides[$app]['stack_size']) {
        return $appOverrides[$app]['stack_size']
    }
    return ""
}

function Get-IgnoreApp {
    param([string]$app)
    return ($appOverrides.ContainsKey($app) -and $appOverrides[$app]['ignore'])
}

# CP/M data fixtures are every file in tests/ that is not a C source or repo
# metadata (.c, .json, .md, dotfiles). This is filesystem-independent: it does
# not depend on the case of the stored filename, so it behaves the same on
# case-sensitive (Linux) and case-insensitive (macOS, Windows) checkouts.
function Get-FixtureFiles {
    if (-not (Test-Path "tests" -PathType Container)) { return @() }
    return @(Get-ChildItem -Path "tests" -File |
        Where-Object { $_.Name -notlike '.*' -and $_.Extension -notin '.c', '.json', '.md' } |
        ForEach-Object { $_.Name })
}

# Stage every CP/M data fixture into the shared build dir (serial mode). Some
# interpreters read these files from the current working directory, and apps are
# run from the build dir (below), so the fixtures must live there too.
function Stage-FixtureInputs {
    foreach ($f in $fixtureList) {
        Copy-FixtureUpper -Name $f -DestDir $BuildDir
    }
}

# Stage one CP/M data fixture into a run directory.
#
# CP/M (and the ntvcm emulator) uppercases every filename a program opens, so a
# fixture must be present under its UPPERCASE name in the run directory to be
# found on case-sensitive filesystems (Linux). macOS and Windows filesystems are
# case-insensitive, so the uppercase name resolves there too. The source file is
# resolved case-insensitively, so a single canonical copy in tests/ (in any
# case, e.g. lowercase eu.c) is sufficient on every platform -- no need to commit
# duplicate uppercase copies (which also collide in git on case-insensitive
# checkouts).
function Copy-FixtureUpper {
    param([string]$Name, [string]$DestDir)
    $dest = Join-Path $DestDir ($Name.ToUpper())
    foreach ($dir in @("tests", ".")) {
        if (-not (Test-Path $dir -PathType Container)) { continue }
        $src = Get-ChildItem -LiteralPath $dir -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -ieq $Name } | Select-Object -First 1
        if ($src) {
            Copy-Item -LiteralPath $src.FullName -Destination $dest -Force -ErrorAction SilentlyContinue
            return
        }
    }
}

# Build, run, and verify a single app across the requested modes. Returns a
# result object with a collected log (Lines) so callers can print output in a
# deterministic, grouped order (important under parallel execution). This is
# self-contained: it only depends on Invoke-MaBuild / ConvertTo-CRLF (from
# ma.ps1) and Test-MatchesBaseline, all of which are made available in both
# serial and parallel contexts.
function Invoke-AppTest {
    param(
        [string]$AppName,
        [string[]]$Modes,
        [string]$BuildDir,
        [string]$BaselineDir,
        [string]$Emulator,
        [System.Collections.IDictionary]$Placeholders,
        [string]$RunArgs,
        [string]$StackSize,
        [string[]]$Fixtures
    )

    $lines = [System.Collections.Generic.List[string]]::new()
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $appPassed = $true

    # Ensure the (possibly per-app) build dir exists and stage CP/M fixtures.
    if (-not (Test-Path $BuildDir -PathType Container)) {
        New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
    }
    foreach ($f in $Fixtures) {
        Copy-FixtureUpper -Name $f -DestDir $BuildDir
    }

    foreach ($buildMode in $Modes) {
        if ($StackSize) { $env:DCC_STACK_SIZE = $StackSize }
        $ok = $false
        try {
            $ok = Invoke-MaBuild -Name $AppName -Mode $buildMode -BuildDir $BuildDir -Emulator $Emulator -Quiet
        }
        catch { $ok = $false }
        finally {
            if ($StackSize) { Remove-Item env:DCC_STACK_SIZE -ErrorAction SilentlyContinue }
        }

        if (-not $ok) {
            $lines.Add("  Building $AppName ($buildMode)... FAILED")
            $appPassed = $false
            break
        }
        $lines.Add("  Building $AppName ($buildMode)... done")

        # Run from the build dir so interpreters find their staged data fixtures.
        $upper = $AppName.ToUpper()
        $comFile = Join-Path $BuildDir "$upper.COM"
        if (-not (Test-Path $comFile)) {
            $lines.Add("    WARNING: $comFile not found, skipping execution")
            $appPassed = $false
            continue
        }

        Push-Location $BuildDir
        try {
            if ($AppName -eq "tkbd") {
                $out = "x" | & $Emulator "$upper.COM" 2>&1
            }
            elseif ($RunArgs) {
                $al = $RunArgs -split '\s+'
                $out = & $Emulator "$upper.COM" @al 2>&1
            }
            else {
                $out = & $Emulator "$upper.COM" 2>&1
            }
            $output = ($out -join "`n")
        }
        catch {
            $output = ""
            $lines.Add("    ERROR running $AppName : $_")
        }
        finally {
            Pop-Location
        }

        # Compare against the per-app baseline (placeholder-aware).
        $blPath = Join-Path $BaselineDir "$AppName.txt"
        if (Test-Path $blPath -PathType Leaf) {
            $expected = (((Get-Content -Path $blPath -Raw) -replace "`r`n", "`n")).TrimEnd("`n")
            $actual = ($output -replace "`r`n", "`n").TrimEnd("`n")
            if (-not (Test-MatchesBaseline -Actual $actual -Baseline $expected -Placeholders $Placeholders)) {
                $lines.Add("    OUTPUT MISMATCH (vs $BaselineDir/$AppName.txt)")
                $lines.Add("    Got: " + (($actual -split "`n" | Select-Object -First 3) -join ' | '))
                $appPassed = $false
            }
            else {
                $lines.Add("    Output matches baseline")
            }
        }
        else {
            $lines.Add("    ERROR: no baseline at $BaselineDir/$AppName.txt (every app must have one)")
            $appPassed = $false
        }
    }

    $sw.Stop()
    return [pscustomobject]@{
        App     = $AppName
        Passed  = $appPassed
        Elapsed = $sw.Elapsed
        Lines   = $lines.ToArray()
    }
}

# Per-app baseline files live in $BaselineDir (one <app>.txt per test, holding
# that app's exact expected stdout). Comparison is by app name, so the order in
# which tests are discovered/run does not matter. See tests/README.md.
$baselineAvailable = Test-Path $BaselineDir -PathType Container
if ($baselineAvailable) {
    $baselineCount = @(Get-ChildItem -Path "$BaselineDir/*.txt" -File -ErrorAction SilentlyContinue).Count
    Write-Host "Using per-app baselines from $BaselineDir ($baselineCount files)" -ForegroundColor Cyan
} else {
    Write-Host "No baseline directory at $BaselineDir; running without verification" -ForegroundColor Yellow
}

function Get-Baseline {
    param([string]$app)
    $path = Join-Path $BaselineDir "$app.txt"
    if (Test-Path $path -PathType Leaf) {
        # Return raw expected output, normalized to LF.
        return ((Get-Content -Path $path -Raw) -replace "`r`n", "`n")
    }
    return $null
}

# Global placeholder definitions for volatile output. A baseline file may embed
# any of these tokens; at compare time the baseline is turned into a regex
# template and matched against the actual output. This keeps ALL expected
# content in the single baseline file (no per-app rules elsewhere) while still
# tolerating values that legitimately change between builds/platforms.
#
#   {{DATE}} - C __DATE__ value, e.g. "Jun 16 2026"
#   {{TIME}} - C __TIME__ value, e.g. "20:01:50"
#   {{SEP}}  - path separator, "/" (Unix) or "\" (Windows)
$Placeholders = [ordered]@{
    '{{DATE}}' = '[A-Z][a-z]{2}\s+\d{1,2}\s+\d{4}'
    '{{TIME}}' = '\d{2}:\d{2}:\d{2}'
    '{{SEP}}'  = '[/\\]'
}

# Compare actual output against a baseline that may contain placeholder tokens.
# If the baseline has no placeholders, this is a plain string equality check.
# Placeholders are passed explicitly so this works inside parallel runspaces
# (which do not inherit the parent's script-scope variables).
function Test-MatchesBaseline {
    param(
        [string]$Actual,
        [string]$Baseline,
        [System.Collections.IDictionary]$Placeholders
    )
    if (-not $Placeholders) { $Placeholders = $script:Placeholders }
    if ($Baseline -notmatch '\{\{[A-Z]+\}\}') {
        return ($Actual -ceq $Baseline)
    }
    # Build a regex from the baseline: escape literal text segments, and insert
    # each placeholder's pattern in place of its token. Escaping only the
    # literal parts avoids double-escaping the tokens themselves.
    $tokenRegex = [regex]'\{\{([A-Z]+)\}\}'
    $sb = [System.Text.StringBuilder]::new()
    $last = 0
    foreach ($m in $tokenRegex.Matches($Baseline)) {
        $literal = $Baseline.Substring($last, $m.Index - $last)
        [void]$sb.Append([regex]::Escape($literal))
        $token = '{{' + $m.Groups[1].Value + '}}'
        if ($Placeholders.Contains($token)) {
            [void]$sb.Append($Placeholders[$token])
        }
        else {
            # Unknown token: treat it literally.
            [void]$sb.Append([regex]::Escape($m.Value))
        }
        $last = $m.Index + $m.Length
    }
    [void]$sb.Append([regex]::Escape($Baseline.Substring($last)))
    return ($Actual -cmatch ('^' + $sb.ToString() + '$'))
}

# Main build and run loop
$passed = 0
$failed = 0
$skipped = 0
$failedApps = @()

$modes = if ($Mode -eq "both") { @("peep", "nopeep") } else { @($Mode) }

# The CP/M data fixtures are derived once from the tests/ directory (see
# Get-FixtureFiles). Every app gets the same set staged into its run dir.
$fixtureList = @(Get-FixtureFiles)

# Build the list of work items up front (resolving per-app args/stack in the
# parent), so parallel workers don't need the $appOverrides table.
$workItems = [System.Collections.Generic.List[object]]::new()
foreach ($app in $testFiles) {
    if (Get-IgnoreApp $app) {
        $skipped++
        continue
    }
    $workItems.Add([pscustomobject]@{
        App          = $app
        RunArgs      = (Get-AppArgs $app)
        StackSize    = (Get-StackSize $app)
    })
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "STARTING BUILD AND RUN SUITE" -ForegroundColor Cyan
if ($Parallel) {
    Write-Host "(parallel, throttle = $ThrottleLimit)" -ForegroundColor Cyan
}
Write-Host "========================================" -ForegroundColor Cyan

# Measure total suite time
$suiteStopwatch = [System.Diagnostics.Stopwatch]::StartNew()

# Helper to render one app's collected result.
function Show-AppResult {
    param($result)
    Write-Host ""
    Write-Host "Testing $($result.App)..." -ForegroundColor Yellow
    foreach ($line in $result.Lines) {
        $color = if ($line -match 'FAILED|MISMATCH|WARNING|ERROR') { 'Red' }
                 elseif ($line -match 'matches baseline') { 'Green' }
                 else { 'Gray' }
        Write-Host $line -ForegroundColor $color
    }
    $elapsed = $result.Elapsed
    $elapsedStr = if ($elapsed.TotalSeconds -ge 60) { "{0:m\m\ s\.f\s}" -f $elapsed } else { "{0:0.00}s" -f $elapsed.TotalSeconds }
    $scTag = if ($StackCheck) { "Stack Check Enabled" } else { "No Stack Check" }
    $status = if ($result.Passed) { "PASS" } else { "FAIL" }
    $line = "  {0}  {1,-12} {2,8} | {3}" -f $status, $result.App, $elapsedStr, $scTag
    Write-Host $line -ForegroundColor $(if ($result.Passed) { "Green" } else { "Red" })
}

$results = @()
$totalToRun = $workItems.Count

if ($Parallel) {
    # Each worker runs in its own runspace with its own filesystem location, so
    # per-app build dirs (build/<app>) prevent the shared-file clobbering that
    # would otherwise occur (DCCRTL.MAC, RTLMIN.MAC, tool COMs, .COM outputs).
    $repoRoot     = (Get-Location).Path
    $maPath       = (Join-Path $PSScriptRoot "ma.ps1")
    $tmbDef       = ${function:Test-MatchesBaseline}.ToString()
    $iatDef       = ${function:Invoke-AppTest}.ToString()
    $cfuDef       = ${function:Copy-FixtureUpper}.ToString()
    $stackCheckOn = [bool]$StackCheck

    # ForEach-Object -Parallel streams each worker's result as it completes, so
    # pipe straight into a loop that prints a live status line per app. Results
    # arrive in completion order (not sorted); we collect them for the summary.
    $done = 0
    $workItems | ForEach-Object -ThrottleLimit $ThrottleLimit -Parallel {
        $item = $_
        Set-Location $using:repoRoot
        if ($using:stackCheckOn) { $env:DCC_FORCE_STACK_CHECK = "1" }
        # Bring the needed functions into this runspace.
        . $using:maPath                                   # Invoke-MaBuild, ConvertTo-CRLF
        ${function:Test-MatchesBaseline} = $using:tmbDef
        ${function:Copy-FixtureUpper}    = $using:cfuDef
        ${function:Invoke-AppTest}       = $using:iatDef

        $appBuildDir = Join-Path $using:BuildDir $item.App
        Invoke-AppTest -AppName $item.App -Modes $using:modes -BuildDir $appBuildDir `
            -BaselineDir $using:BaselineDir -Emulator $using:Emulator `
            -Placeholders $using:Placeholders -RunArgs $item.RunArgs `
            -StackSize $item.StackSize -Fixtures $using:fixtureList
    } | ForEach-Object {
        $result = $_
        $results += $result
        $done++
        $elapsed = $result.Elapsed
        $elapsedStr = if ($elapsed.TotalSeconds -ge 60) { "{0:m\m\ s\.f\s}" -f $elapsed } else { "{0:0.00}s" -f $elapsed.TotalSeconds }
        $counter = "[{0,3}/{1}]" -f $done, $totalToRun
        $scTag = if ($StackCheck) { "Stack Check Enabled" } else { "No Stack Check" }
        $status = if ($result.Passed) { "PASS" } else { "FAIL" }
        # Columns: counter | status | app | time | run-wide stack-check tag
        $line = "{0} {1}  {2,-12} {3,8} | {4}" -f $counter, $status, $result.App, $elapsedStr, $scTag
        if ($result.Passed) {
            Write-Host $line -ForegroundColor Green
        } else {
            Write-Host $line -ForegroundColor Red
            # Show the failing detail lines inline so problems are visible live.
            foreach ($detail in $result.Lines) {
                if ($detail -match 'FAILED|MISMATCH|WARNING|ERROR') {
                    Write-Host "        $($detail.Trim())" -ForegroundColor Red
                }
            }
        }
    }
}
else {
    # Serial: stage fixtures once into the shared build dir and reuse it.
    Stage-FixtureInputs
    foreach ($item in $workItems) {
        $result = Invoke-AppTest -AppName $item.App -Modes $modes -BuildDir $BuildDir `
            -BaselineDir $BaselineDir -Emulator $Emulator -Placeholders $Placeholders `
            -RunArgs $item.RunArgs -StackSize $item.StackSize -Fixtures $fixtureList
        Show-AppResult $result
        $results += $result
    }
}

# Tally results.
foreach ($result in $results) {
    if ($result.Passed) {
        $passed++
    } else {
        $failed++
        $failedApps += $result.App
    }
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "TEST SUITE SUMMARY" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
$suiteStopwatch.Stop()
$suiteElapsed = $suiteStopwatch.Elapsed
$suiteElapsedStr = if ($suiteElapsed.TotalSeconds -ge 60) {
    "{0:m\m\ s\.f\s}" -f $suiteElapsed
} else {
    "{0:0.00}s" -f $suiteElapsed.TotalSeconds
}
Write-Host "  Total apps:   $($testFiles.Count)"
Write-Host "  Passed:       $passed" -ForegroundColor Green
Write-Host "  Failed:       $failed" -ForegroundColor $(if ($failed -eq 0) { "Green" } else { "Red" })
Write-Host "  Skipped:      $skipped"
Write-Host "  Total time:   $suiteElapsedStr"

if ($failed -gt 0) {
    Write-Host ""
    Write-Host "Failed apps:" -ForegroundColor Red
    foreach ($app in $failedApps) {
        Write-Host "  - $app" -ForegroundColor Red
    }
}

Write-Host ""
if ($null -ne $_savedStty) { stty $_savedStty 2>$null }
if ($failed -eq 0) {
    Write-Host ">>> SUCCESS: All tests passed <<<" -ForegroundColor Green
    exit 0
} else {
    Write-Host ">>> FAILURE: $failed test(s) failed <<<" -ForegroundColor Red
    exit 1
}
