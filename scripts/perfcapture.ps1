#!/usr/bin/env pwsh
# Performance capture script for all test apps (PowerShell 7+)
# Builds and runs all .c files in tests/ in both peep and nopeep modes
# Outputs CSV: machine,utc-timestamp,app,peep_ms,peep_size,nopeep_ms,nopeep_size

param(
    [string]$BuildDir = "build",
    [string]$OutputFile = "perf_results.csv",
    [string]$Emulator = "ntvcm"
)

# Get all test files
$testFiles = @()
if (Test-Path "tests") {
    $testFiles = @(Get-ChildItem -Path "tests" -Filter "*.c" -File | 
                   ForEach-Object { $_.BaseName } | 
                   Sort-Object)
}

if ($testFiles.Count -eq 0) {
    Write-Host "ERROR: No test files found in tests/ directory" -ForegroundColor Red
    exit 1
}

# Load app overrides lookup table from JSON (lives in tests/, alongside the
# test sources it configures). Resolve relative to the repo root (parent of the
# scripts folder) so it works regardless of the current directory.
$repoRoot = Split-Path (Split-Path $PSCommandPath -Parent) -Parent
$appOverridesPath = Join-Path $repoRoot "tests/_test_overrides.json"
$appOverrides = @{}

if (Test-Path $appOverridesPath) {
    $json = Get-Content -Path $appOverridesPath -Raw | ConvertFrom-Json
    foreach ($app in $json.apps) {
        $appOverrides[$app.name] = $app
    }
}

# Create build directory if needed
if (!(Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
}

# Test-specific arguments (from lookup table)
function Get-RunArgs {
    param([string]$app)
    
    if ($appOverrides.ContainsKey($app) -and $appOverrides[$app].args) {
        return $appOverrides[$app].args
    }
    return ""
}

# Stack size overrides (from lookup table)
function Get-StackSize {
    param([string]$app)
    
    $globalStack = $env:STACK_SIZE
    if ($globalStack) { return $globalStack }
    
    if ($appOverrides.ContainsKey($app) -and $appOverrides[$app].stack_size) {
        return $appOverrides[$app].stack_size
    }
    return ""
}

# Check if app should be ignored (from lookup table)
function Get-IgnoreApp {
    param([string]$app)
    
    if ($appOverrides.ContainsKey($app) -and $appOverrides[$app].ignore) {
        return $true
    }
    return $false
}

# Stage fixture input files
function Stage-FixtureInputs {
    $fixtures = @("E.PAS", "E.COB", "E.FOR", "E.ADA", "E.BAS", "E.F", "EU.C")
    foreach ($f in $fixtures) {
        if (Test-Path "tests\$f") {
            Copy-Item -Path "tests\$f" -Destination "$BuildDir\$f" -Force -ErrorAction SilentlyContinue
        }
        elseif (Test-Path $f) {
            Copy-Item -Path $f -Destination "$BuildDir\$f" -Force -ErrorAction SilentlyContinue
        }
    }
}

# Clean build artifacts for an app
function Clean-One {
    param([string]$app)
    
    $upper = $app.ToUpper()
    $files = @(
        "$BuildDir\$upper.MAC",
        "$BuildDir\$upper.REL",
        "$BuildDir\$upper.PRN",
        "$BuildDir\$upper.COM",
        "$BuildDir\$app.mac",
        "$BuildDir\$app.rel",
        "$BuildDir\$app.prn",
        "$BuildDir\$app.com",
        "$BuildDir\$app.COM"
    )
    
    foreach ($f in $files) {
        if (Test-Path $f) {
            Remove-Item -Path $f -Force -ErrorAction SilentlyContinue
        }
    }
}

# Run build and benchmark for a given mode
function Run-Mode {
    param(
        [string]$mode,
        [hashtable]$results
    )
    
    Write-Host "Capturing $mode benchmarks..."
    
    Stage-FixtureInputs
    
    $count = 0
    foreach ($app in $testFiles) {
        $upper = $app.ToUpper()
        
        # Skip ignored apps
        if (Get-IgnoreApp $app) {
            Write-Host "  Skipping $app (ignored)"
            continue
        }
        
        Write-Host "  Building $app... " -NoNewline
        Clean-One $app
        
        # Get stack size override if any
        $stackSz = Get-StackSize $app
        
        try {
            if ($stackSz) {
                $env:DCC_STACK_SIZE = $stackSz
                & .\ma.sh $app $mode *> $null
                $env:DCC_STACK_SIZE = ""
            }
            else {
                & .\ma.sh $app $mode *> $null
            }
        }
        catch {
            Write-Host "FAILED"
            Clean-One $app
            continue
        }
        
        Write-Host "done"
        
        # Get binary size
        $comFile = "$BuildDir\$upper.COM"
        if (!(Test-Path $comFile)) {
            Write-Host "    WARNING: $comFile not found, skipping"
            Clean-One $app
            continue
        }
        
        $size = (Get-Item $comFile).Length
        
        # Handle interactive test (tkbd)
        if ($app -eq "tkbd") {
            Write-Host "  Running $app (interactive)... " -NoNewline
            Write-Host "x" | & $Emulator $upper *> $null
            $ms = 0
        }
        else {
            Write-Host "  Running $app... " -NoNewline
            
            $args = Get-RunArgs $app
            
            # Time the execution
            $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
            
            if ($args) {
                & $Emulator $upper $args.Split() *> $null
            }
            else {
                & $Emulator $upper *> $null
            }
            
            $stopwatch.Stop()
            $ms = $stopwatch.ElapsedMilliseconds
        }
        
        Write-Host "done (${ms}ms, ${size}B)"
        
        # Store result
        if (-not $results.ContainsKey($app)) {
            $results[$app] = @{}
        }
        $results[$app][$mode] = @{
            ms = $ms
            size = $size
        }
        
        Clean-One $app
        
        $count++
        if ($count % 10 -eq 0) {
            Write-Host "    Progress: $count apps processed"
        }
    }
    
    Write-Host "    Total: $count apps in $mode mode"
}

# Main script
$results = @{}

# Capture both modes
Run-Mode "peep" $results
Write-Host ""
Run-Mode "nopeep" $results
Write-Host ""

# Generate UTC timestamp
$utcTimestamp = [System.DateTime]::UtcNow.ToString("yyyy-MM-ddTHH:mm:ssZ")

# Get machine name (platform-specific)
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

# Create header if file doesn't exist
$headerExists = Test-Path $OutputFile
if (!$headerExists) {
    Add-Content -Path $OutputFile -Value "machine,utc-timestamp,app,peep_ms,peep_size,nopeep_ms,nopeep_size"
}

# Merge results into CSV
Write-Host "Merging results..."
$appCount = 0

foreach ($app in $testFiles) {
    if ($results.ContainsKey($app) -and 
        $results[$app].ContainsKey("peep") -and 
        $results[$app].ContainsKey("nopeep")) {
        
        $peepMs = $results[$app]["peep"].ms
        $peepSize = $results[$app]["peep"].size
        $nopeepMs = $results[$app]["nopeep"].ms
        $nopeepSize = $results[$app]["nopeep"].size
        
        $csvLine = "$machineName,$utcTimestamp,$app,$peepMs,$peepSize,$nopeepMs,$nopeepSize"
        Add-Content -Path $OutputFile -Value $csvLine
        $appCount++
    }
}

Write-Host ""
Write-Host "Results saved to: $OutputFile ($appCount apps)"
Write-Host ""

# Show last 10 rows
if (Test-Path $OutputFile) {
    @(Get-Content $OutputFile) | Select-Object -Last 10 | Write-Host
}
