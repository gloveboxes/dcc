echo off
setlocal enabledelayedexpansion

:: Resolve the directory this script lives in and the repo root
cd /d "%~dp0"
set "SCRIPT_DIR=%CD%"
cd ..\..
set "REPO_ROOT=%CD%"

:: MSVC compiler flags (Wall/Wextra mapped to MSVC equivalents, O2 optimization)
set "CC=cl"
set "CFLAGS=/nologo /GS- /GL /Oti2 /Ob3 /Qpar /Fa /FAsc /Zi /std:c11"
set "OUT=%REPO_ROOT%\dcc.exe"

:: Allow "-o <path>" to override the output location
if "%~1"=="-o" (
    if not "%~2"=="" (
        set "OUT=%~2"
    )
)

echo Building modular dcc -> %OUT%

:: Compile and link all .c files with MSVC
cd /d "%SCRIPT_DIR%"
%CC% %CFLAGS% /I. /Fe:"%OUT%" *.c

echo Done.

