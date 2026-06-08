@echo off
setlocal

if "%~1"=="" (
    call ma.bat tstring
    call ma.bat sieve
    call ma.bat e
    call ma.bat tm
    call ma.bat ttt
    call ma.bat pihex
    call ma.bat mm
) else (
    call ma.bat tstring nopeep
    call ma.bat sieve nopeep
    call ma.bat e nopeep
    call ma.bat tm nopeep
    call ma.bat ttt nopeep
    call ma.bat pihex nopeep
    call ma.bat mm nopeep
)

ntvcm -c -p tstring
ntvcm -c -p sieve
ntvcm -c -p e
ntvcm -c -p tm
ntvcm -c -p ttt 10
ntvcm -c -p pihex
ntvcm -c -p mm

rem dir /OD tstring.com sieve.com e.com tm.com ttt.com pihex.com mm.com

@echo off
for %%i in (tstring.com sieve.com e.com tm.com ttt.com pihex.com mm.com) do (
    for /f "tokens=*" %%a in ('dir /OD "%%i" ^| findstr /R "^[0-9]"') do echo %%a
)



