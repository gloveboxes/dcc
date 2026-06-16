@echo off
setlocal EnableDelayedExpansion EnableExtensions

REM Performance capture script for benchmark apps
REM Builds and runs apps in both peep and nopeep modes
REM Outputs CSV: utc-timestamp,app,peep_ms,peep_size,nopeep_ms,nopeep_size

set "APPS=tstring sieve e tm ttt pihex mm"
set "BUILD_DIR=build"
set "OUTPUT_FILE=perf_results.csv"
set "EMULATOR=ntvcm"

if not exist "!BUILD_DIR!" mkdir "!BUILD_DIR!"

REM Create temp files for storing intermediate results
for /f "tokens=2-4 delims=/ " %%a in ('date /t') do (set mydate=%%c%%a%%b)
for /f "tokens=1-2 delims=/:" %%a in ('time /t') do (set mytime=%%a%%b)
set "PEEP_RESULTS=%TEMP%\peep_results_!mydate!_!mytime!.tmp"
set "NOPEEP_RESULTS=%TEMP%\nopeep_results_!mydate!_!mytime!.tmp"

REM Run capture for each mode
call :run_mode peep "!PEEP_RESULTS!"
echo.
call :run_mode nopeep "!NOPEEP_RESULTS!"
echo.

REM Get current UTC timestamp
for /F "usebackq" %%D in (`powershell -Command "[System.DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')"`) do set "utc_timestamp=%%D"

REM Create output CSV with header
echo utc-timestamp,app,peep_ms,peep_size,nopeep_ms,nopeep_size > "!OUTPUT_FILE!"

echo Merging results...
for %%A in (!APPS!) do (
    set "app=%%A"
    set "peep_ms="
    set "peep_size="
    set "nopeep_ms="
    set "nopeep_size="
    
    REM Read peep results
    for /f "tokens=1,2,3 delims=:" %%1 in ('findstr "^!app!:" "!PEEP_RESULTS!" 2^>nul') do (
        set "peep_ms=%%2"
        set "peep_size=%%3"
    )
    
    REM Read nopeep results
    for /f "tokens=1,2,3 delims=:" %%1 in ('findstr "^!app!:" "!NOPEEP_RESULTS!" 2^>nul') do (
        set "nopeep_ms=%%2"
        set "nopeep_size=%%3"
    )
    
    if defined peep_ms if defined nopeep_ms (
        echo !utc_timestamp!,!app!,!peep_ms!,!peep_size!,!nopeep_ms!,!nopeep_size! >> "!OUTPUT_FILE!"
    ) else (
        echo   WARNING: Missing data for !app!
    )
)

REM Cleanup temp files
if exist "!PEEP_RESULTS!" del "!PEEP_RESULTS!"
if exist "!NOPEEP_RESULTS!" del "!NOPEEP_RESULTS!"

echo.
echo Results saved to: !OUTPUT_FILE!
type "!OUTPUT_FILE!"

endlocal
exit /b 0

:run_mode
setlocal EnableDelayedExpansion EnableExtensions
set "mode=%~1"
set "results_file=%~2"

echo Capturing !mode! benchmarks...

REM Clear results file
> "!results_file!"

REM Stage fixture inputs
for %%F in (E.PAS E.COB E.FOR E.ADA E.BAS E.F EU.C) do (
    if exist "tests\%%F" (
        copy /Y "tests\%%F" "!BUILD_DIR!\%%F" >nul 2>&1
    ) else if exist "%%F" (
        copy /Y "%%F" "!BUILD_DIR!\%%F" >nul 2>&1
    )
)

REM Process each app
for %%A in (!APPS!) do (
    set "app=%%A"
    for /F "usebackq" %%U in (`powershell -Command "[%%A].ToUpper()"`) do set "upper=%%U"
    
    echo   Building !app!...
    call ma.bat !app! !mode! >nul 2>&1
    if errorlevel 1 (
        echo     ERROR: Failed to build !app!
        goto :run_mode_next_app
    )
    
    REM Get binary size
    set "com_file=!BUILD_DIR!\!upper!.COM"
    if not exist "!com_file!" (
        echo     WARNING: !com_file! not found, skipping
        goto :run_mode_next_app
    )
    
    for %%F in ("!com_file!") do set "size=%%~zF"
    
    echo   Running !app!...
    
    REM Run with PowerShell timing to get milliseconds
    for /F "usebackq" %%T in (`powershell -Command "^$sw = [System.Diagnostics.Stopwatch]::StartNew(); ^& !EMULATOR! !upper! >nul 2>&1; ^$sw.Stop(); Write-Host ^$sw.ElapsedMilliseconds"`) do set "ms=%%T"
    
    echo     Done (^!ms^!ms, ^!size^!B)
    echo !app!:!ms!:!size! >> "!results_file!"
    
    :run_mode_next_app
)

endlocal
exit /b 0
