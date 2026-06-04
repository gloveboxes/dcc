@echo off
setlocal EnableExtensions

if "%~1"=="" (
    echo usage: mlz lzpack [peep^|nopeep]
    echo example: mlz lzpack peep
    echo example: mlz lzpack nopeep
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

set "DCC_FLAGS="
set "STRIP_FLAGS="

rem Enable float printf only when the source appears to use %f/%F formatting.
rem This keeps ordinary binaries smaller because _pffio and its helpers are not rooted.
findstr /I /C:"%%f" /C:"%%.f" /C:"%%0f" /C:"%%1f" /C:"%%2f" /C:"%%3f" /C:"%%4f" /C:"%%5f" /C:"%%6f" /C:"%%7f" /C:"%%8f" /C:"%%9f" "%SOURCE_FILE%" >nul 2>&1
if not errorlevel 1 (
    set "DCC_FLAGS=%DCC_FLAGS% -ffloatio"
    set "STRIP_FLAGS=%STRIP_FLAGS% -k _pffio"
)

findstr /I /C:"%%F" /C:"%%.F" /C:"%%0F" /C:"%%1F" /C:"%%2F" /C:"%%3F" /C:"%%4F" /C:"%%5F" /C:"%%6F" /C:"%%7F" /C:"%%8F" /C:"%%9F" "%SOURCE_FILE%" >nul 2>&1
if not errorlevel 1 (
    set "DCC_FLAGS=%DCC_FLAGS% -ffloatio"
    set "STRIP_FLAGS=%STRIP_FLAGS% -k _pffio"
)

rem Compile on host first, producing %name%.mac.
dcc.exe -DHSZ=512 -DMZXFILE=65535L -D__Z80 -DLZPACK_STREAM=1 -DCPM80 %DCC_FLAGS% -stack 384 "%SOURCE_FILE%" -o "%name%.mac"
if errorlevel 1 exit /b 1

if "%USE_PEEP%"=="1" (
    rem optimize for size, not speed (the default -Ot)
    dccpeep -Os "%name%.mac" _peepout.mac
    if errorlevel 1 exit /b 1
    del "%name%.mac"
    ren _peepout.mac "%name%.mac"
)

rem Ensure CRLF line endings so CP/M M80 doesn't split on embedded LF bytes.
unix2dos "%name%.mac" >nul 2>nul

rem Assemble app.
ntvcm m80 =%name%.mac /X /O /Z /L
if errorlevel 1 exit /b 1

rem Produce a subset of the C runtime actually used by the app.
unix2dos DCCRTL.MAC >nul 2>nul
dccrtlstrip.exe %STRIP_FLAGS% -r dccrtl.mac -o rtlmin.mac "%name%.mac"
if errorlevel 1 exit /b 1

rem Assemble runtime.
unix2dos rtlmin.mac >nul 2>nul
ntvcm m80 =rtlmin.mac /X /O /Z
if errorlevel 1 exit /b 1

rem Link app + runtime.
ntvcm l80 /P:100,rtlmin,%name%,%name%/N/E
if errorlevel 1 exit /b 1



