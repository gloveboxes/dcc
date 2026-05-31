@echo off
setlocal EnableExtensions

if "%~1"=="" (
    echo usage: mrel name [peep^|nopeep]
    echo example: mrel qsort peep
    echo example: mrel qsort nopeep
    exit /b 1
)

set "name=%~n1"
set "SOURCE_FILE=%name%.c"
set "USE_PEEP=1"

if /I "%~2"=="nopeep" set "USE_PEEP=0"
if /I "%~2"=="nop" set "USE_PEEP=0"
if /I "%~2"=="noopt" set "USE_PEEP=0"
if /I "%~2"=="u" set "USE_PEEP=0"
if /I "%~2"=="-u" set "USE_PEEP=0"
if /I "%~2"=="peep" set "USE_PEEP=1"
if /I "%~2"=="opt" set "USE_PEEP=1"
if /I "%~2"=="-O" set "USE_PEEP=1"

if not exist "%SOURCE_FILE%" (
    echo source file not found: %SOURCE_FILE%
    exit /b 1
)

dcc -c "%SOURCE_FILE%" -o %name%.mac
if errorlevel 1 exit /b 1

if "%USE_PEEP%"=="1" (
    dccpeep "%name%.mac" _peepout.mac
    if errorlevel 1 exit /b 1
    del "%name%.mac"
    ren _peepout.mac "%name%.mac"
)

rem Ensure CRLF line endings so CP/M M80 doesn't split on embedded LF bytes.
unix2dos "%name%.mac" >nul 2>nul

rem Assemble the module
ntvcm m80 =%name%.mac /X /O /Z /L
if errorlevel 1 exit /b 1

