@echo off
setlocal EnableExtensions

:: Args: [emulator] [--stack-check]
::   --stack-check  build every app with dcc -fstack-check (lightweight
::                  stack-overflow guard).  Also enabled by STACK_CHECK=1.
set "_emulator="
set "_force_stack_check=%STACK_CHECK%"
for %%A in (%*) do (
    if /I "%%~A"=="--stack-check" (
        set "_force_stack_check=1"
    ) else if /I "%%~A"=="-stack-check" (
        set "_force_stack_check=1"
    ) else if not defined _emulator (
        set "_emulator=%%~A"
    )
)
if "%_emulator%"=="" set "_emulator=ntvcm"
if "%_force_stack_check%"=="1" (
    set "DCC_FORCE_STACK_CHECK=1"
    echo --- stack-check: building every app with -fstack-check ---
)
:: the cpm and ntvcm emulators produce identical results
set "_builddir=build"
if not exist "%_builddir%" mkdir "%_builddir%"

set _applist=sieve e ttt tstruct trw tstr tbug tprintf ts tcmp tunary tlong ^
             tpi mm tm tfio wumpus triangle fileops nqueens fact tsetjmp ^
             tenum tunion tgoto tarray tchess targs tstdc tvariad tsprintf tscanf ^
             tdowhile tmuldiv tbdos tdirent tc89core tc89uac tc89init tc89fp ^
             tc89ptr tc89size tc89decl tc89qual tc89comp tc89swjt tc89bit ^
             tc89pp tc89fnty tc89flt tc89fltc tc89flta tc89fptr tc89fs tc89fcmp ^
             tc89fcnv tc89fadd tc89fmul tc89fdiv tc89ffio tc89flng tc89fmat ^
             ttrig tlog tphi tap cpmenumd tbits tfo pihex tstrify tlcont primes ^
             tpreproc trwold tlimits spsmash tcrcfix trtl2 tsyntax tstr2 tstr3 tstring ^
             tlongaud tlongreg tlongopt tctxops tppreg tinitreg ttypesr ttype2 tdecinit tmalloch ^
             tallocx tstdlib tioerr tqsort tbsearch trw2 terrno tpostfld tswitch tppifcom tpostidx ^
             tpostut tbug2 tlongsub treg tret tstructv tstructi tstructp tstri2 ^
             tunion2 tbitfld tcnstfld tpromo tkandr tc89ini2 tdecl tctype tifcom ^
             tptrdiff tmulpow2 toffset tc89fini tmod3216 tpromo2 tunaryp tstfield ^
             pint cobint forint bint fint cint adaint tstretst tportio tlongidx tforsco ^
             tforblk tcmt99 tc99scpe tctxflt tmathf tstrconv tfarrsub t2darr too tzpad tesc ^
             tkbd tstackov

echo --- optimized (ma peep) ---
set "outputfile=test_dcc.txt"
type nul >"%outputfile%"
call :RUNTESTS peep "%outputfile%"

echo --- unoptimized (ma nopeep) ---
set "outputfileu=test_dccu.txt"
type nul >"%outputfileu%"
call :RUNTESTS nopeep "%outputfileu%"

:: Compare the results to verify optimization consistency.
dos2unix "%outputfile%" >nul 2>&1
diff baseline_%outputfile% "%outputfile%"
dos2unix "%outputfileu%" >nul 2>&1
diff baseline_%outputfile% "%outputfileu%"

goto :EOF

:RUNTESTS
set "peepmode=%~1"
set "outfile=%~2"
set "outabs=%cd%\%outfile%"

call :STAGEFIXTURES

for %%a in (%_applist%) do (
    echo test %%a
    echo test %%a>>"%outfile%"

    if exist "%_builddir%\%%a.com" del "%_builddir%\%%a.com"

    rem Per-app C stack reserve (bytes).  Most apps use the dcc default (512);
    rem a few recursive ones need more headroom, which only matters under
    rem stack-check.  A global STACK_SIZE env var overrides this for every app.
    rem   triangle ~626 -> 768 ; cobint ~1376 -> 1536
    rem   spsmash has NO passing size (factorial(4e9) smashes the stack on
    rem   purpose), so it is left at the default.
    set "DCC_STACK_SIZE="
    if defined STACK_SIZE (
        set "DCC_STACK_SIZE=%STACK_SIZE%"
    ) else if "%%a"=="triangle" (
        set "DCC_STACK_SIZE=768"
    ) else if "%%a"=="cobint" (
        set "DCC_STACK_SIZE=1536"
    )

    call ma %%a %peepmode% >nul
    if errorlevel 1 exit /b 1
    set "DCC_STACK_SIZE="

    if "%%a"=="ttt" (
        pushd "%_builddir%" && %_emulator% %%a 10 >>"%outabs%" & popd
    ) else if "%%a"=="tkbd" (
        pushd "%_builddir%" && echo x | %_emulator% %%a >>"%outabs%" & popd
    ) else if "%%a"=="wumpus" (
        pushd "%_builddir%" && %_emulator% %%a -c >>"%outabs%" & popd
    ) else if "%%a"=="tchess" (
        pushd "%_builddir%" && %_emulator% %%a -c >>"%outabs%" & popd
    ) else if "%%a"=="pint" (
        pushd "%_builddir%" && %_emulator% %%a e.pas >>"%outabs%" & popd
    ) else if "%%a"=="cobint" (
        pushd "%_builddir%" && %_emulator% %%a e.cob >>"%outabs%" & popd
    ) else if "%%a"=="forint" (
        pushd "%_builddir%" && %_emulator% %%a e.for >>"%outabs%" & popd
    ) else if "%%a"=="adaint" (
        pushd "%_builddir%" && %_emulator% %%a e.ada >>"%outabs%" & popd
    ) else if "%%a"=="bint" (
        pushd "%_builddir%" && %_emulator% %%a e.bas >>"%outabs%" & popd
    ) else if "%%a"=="fint" (
        pushd "%_builddir%" && %_emulator% %%a e.f >>"%outabs%" & popd
    ) else if "%%a"=="cint" (
        pushd "%_builddir%" && %_emulator% %%a eu.c >>"%outabs%" & popd
    ) else if "%%a"=="targs" (
        pushd "%_builddir%" && %_emulator% %%a a bb ccc dddd eeeee >>"%outabs%" & popd
    ) else (
        pushd "%_builddir%" && %_emulator% %%a >>"%outabs%" & popd
    )

    if exist "%_builddir%\%%a.mac" del "%_builddir%\%%a.mac"
    if exist "%_builddir%\%%a.prn" del "%_builddir%\%%a.prn"
    if exist "%_builddir%\%%a.rel" del "%_builddir%\%%a.rel"
    if exist "%_builddir%\%%a.com" del "%_builddir%\%%a.com"
    if exist "%_builddir%\DCCRTL.MAC" del "%_builddir%\DCCRTL.MAC"
    if exist "%_builddir%\RTLMIN.MAC" del "%_builddir%\RTLMIN.MAC"
    if exist "%_builddir%\RTLMIN.REL" del "%_builddir%\RTLMIN.REL"
    if exist "%_builddir%\RTLMIN.PRN" del "%_builddir%\RTLMIN.PRN"
    if exist "%_builddir%\_PEEPOUT.MAC" del "%_builddir%\_PEEPOUT.MAC"
)
exit /b

:STAGEFIXTURES
if exist "tests\E.PAS" (
    copy /Y "tests\E.PAS" "%_builddir%\E.PAS" >nul
) else if exist "E.PAS" (
    copy /Y "E.PAS" "%_builddir%\E.PAS" >nul
)
if exist "tests\E.COB" (
    copy /Y "tests\E.COB" "%_builddir%\E.COB" >nul
) else if exist "E.COB" (
    copy /Y "E.COB" "%_builddir%\E.COB" >nul
)
if exist "tests\E.FOR" (
    copy /Y "tests\E.FOR" "%_builddir%\E.FOR" >nul
) else if exist "E.FOR" (
    copy /Y "E.FOR" "%_builddir%\E.FOR" >nul
)
if exist "tests\E.ADA" (
    copy /Y "tests\E.ADA" "%_builddir%\E.ADA" >nul
) else if exist "E.ADA" (
    copy /Y "E.ADA" "%_builddir%\E.ADA" >nul
)
if exist "tests\E.BAS" (
    copy /Y "tests\E.BAS" "%_builddir%\E.BAS" >nul
) else if exist "E.BAS" (
    copy /Y "E.BAS" "%_builddir%\E.BAS" >nul
)
if exist "tests\E.F" (
    copy /Y "tests\E.F" "%_builddir%\E.F" >nul
) else if exist "E.F" (
    copy /Y "E.F" "%_builddir%\E.F" >nul
)
if exist "tests\EU.C" (
    copy /Y "tests\EU.C" "%_builddir%\EU.C" >nul
) else if exist "EU.C" (
    copy /Y "EU.C" "%_builddir%\EU.C" >nul
)
exit /b
