@echo off
rem ===========================================================================
rem stacksize.bat - find the minimum C stack reserve an app needs under dcc's
rem lightweight stack-overflow guard (-fstack-check).
rem
rem It builds the app with the guard forced on, then sweeps the -stack reserve
rem upward (default: from 256 bytes in 64-byte steps) until the program runs
rem without tripping the guard ("?stack overflow").  The first size that runs
rem clean is the minimum requirement; the script also prints a rounded-up
rem recommendation with a little headroom.
rem
rem Usage:
rem   scripts\stacksize.bat <app> [-- emulator-args...]
rem
rem   <app>            test/app name as passed to ma.bat (e.g. triangle, cobint)
rem   -- args...       arguments to pass to the emulated program (e.g. a data
rem                    file like e.cob)
rem
rem Options (environment variables):
rem   START=256        first stack size to try (bytes)
rem   STEP=64          increment between tries (bytes)
rem   MAX=8192         give up after this size (apps that overflow on purpose,
rem                    e.g. spsmash, never pass and would loop forever)
rem   MODE=peep        build mode passed to ma.bat (peep|nopeep)
rem   EMU=ntvcm        emulator command used to run the .COM
rem
rem Examples:
rem   scripts\stacksize.bat triangle
rem   scripts\stacksize.bat cobint -- e.cob
rem   set START=512 ^& set STEP=128 ^& scripts\stacksize.bat triangle
rem ===========================================================================
setlocal EnableExtensions EnableDelayedExpansion

set "prog=%~nx0"

rem ---- parse args -----------------------------------------------------------
set "app="
set "EMU_ARGS="
:parse
if "%~1"=="" goto parsed
if /I "%~1"=="-h"     goto usage
if /I "%~1"=="--help" goto usage
if "%~1"=="--" (
    shift
    goto collect_emu_args
)
set "tok=%~1"
if "!tok:~0,1!"=="-" (
    echo %prog%: unknown option: %~1 1>&2
    goto usage_err
)
if not defined app (
    set "app=%~1"
) else (
    set "EMU_ARGS=!EMU_ARGS! %~1"
)
shift
goto parse

:collect_emu_args
if "%~1"=="" goto parsed
set "EMU_ARGS=!EMU_ARGS! %~1"
shift
goto collect_emu_args

:parsed
if not defined app (
    echo %prog%: missing ^<app^> 1>&2
    goto usage_err
)

rem ---- defaults -------------------------------------------------------------
if not defined START set "START=256"
if not defined STEP  set "STEP=64"
if not defined MAX   set "MAX=8192"
if not defined MODE  set "MODE=peep"
if not defined EMU   set "EMU=ntvcm"

rem ---- locate repo root (the parent of this script's dir) -------------------
set "script_dir=%~dp0"
pushd "%script_dir%.." || (echo %prog%: cannot locate repo root 1>&2 & exit /b 1)
set "repo_root=%CD%"

set "BUILD_DIR=build"
if defined BUILDDIR set "BUILD_DIR=%BUILDDIR%"

rem Use the in-repo compiler binaries by default, like the other helper scripts.
if not defined DCC         set "DCC=dcc"
if not defined DCCPEEP     set "DCCPEEP=dccpeep"
if not defined DCCRTLSTRIP set "DCCRTLSTRIP=dccrtlstrip"

rem Force the stack-overflow guard on for every build in this sweep.
set "DCC_FORCE_STACK_CHECK=1"

echo Finding minimum stack for '%app%' (guard on): start=%START% step=%STEP% max=%MAX% mode=%MODE%
if defined EMU_ARGS echo   emulator args:%EMU_ARGS%
echo.

set "found="
set /a "size=%START%"

:sweep
if %size% GTR %MAX% goto done

rem Build with this reserve.
set "DCC_STACK_SIZE=%size%"
call ma.bat "%app%" "%MODE%" >nul 2>&1
if errorlevel 1 (
    call :pad "%size%"
    echo   stack=!padded! : BUILD FAILED
    set /a "size+=%STEP%"
    goto sweep
)

rem Run and capture output (look for the guard message).
set "ovf="
pushd "%BUILD_DIR%"
for /f "delims=" %%L in ('"%EMU%" "%app%"%EMU_ARGS% 2^>^&1') do (
    echo %%L | findstr /C:"?stack overflow" >nul && set "ovf=1"
)
popd

call :pad "%size%"
if defined ovf (
    echo   stack=!padded! : overflow
) else (
    echo   stack=!padded! : OK
    set "found=%size%"
    goto done
)
set /a "size+=%STEP%"
goto sweep

:done
echo.
if not defined found (
    echo No passing stack size up to %MAX% bytes.
    echo This app may overflow on purpose ^(e.g. unbounded recursion^), or raise MAX=... and retry.
    popd
    exit /b 1
)

rem Recommend a rounded-up value with a little headroom (next 128-byte multiple
rem above found + one step).
set /a "rec=%found%+%STEP%"
set /a "rem=rec %% 128"
if not "%rem%"=="0" set /a "rec+=128-rem"

echo Minimum passing stack reserve : %found% bytes
echo Recommended ^(with headroom^)   : %rec% bytes
echo.
echo Build it with:  set DCC_STACK_SIZE=%rec% ^& ma.bat %app% %MODE%
echo Or compile direct:  %DCC% -fstack-check -stack %rec% tests\%app%.c -o %app%.mac
popd
exit /b 0

rem ---- helpers --------------------------------------------------------------
:pad
rem Left-justify a value into a 6-char field stored in %padded%.
set "padded=%~1      "
set "padded=%padded:~0,6%"
exit /b 0

:usage
call :print_usage
exit /b 0

:usage_err
call :print_usage
exit /b 1

:print_usage
echo Usage: scripts\stacksize.bat ^<app^> [-- emulator-args...]
echo.
echo   ^<app^>            test/app name as passed to ma.bat (e.g. triangle, cobint)
echo   -- args...       arguments to pass to the emulated program
echo.
echo Options (environment variables):
echo   START=256        first stack size to try (bytes)
echo   STEP=64          increment between tries (bytes)
echo   MAX=8192         give up after this size
echo   MODE=peep        build mode passed to ma.bat (peep^|nopeep)
echo   EMU=ntvcm        emulator command used to run the .COM
exit /b 0
