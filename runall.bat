@echo off
setlocal EnableExtensions

:: Set the default emulator if no argument is provided.
set "_emulator=%~1"
if "%_emulator%"=="" set "_emulator=ntvcm"
:: the cpm and ntvcm emulators produce identical results

set _applist=sieve e ttt tstruct trw tstr tbug tprintf ts tcmp tunary tlong ^
             tpi mm tm tfio wumpus triangle fileops nqueens fact tsetjmp ^
             tenum tunion tgoto tarray tchess targs tstdc tvariad tsprintf ^
             tdowhile tmuldiv tbdos tdirent tc89core tc89uac tc89init tc89fp ^
             tc89ptr tc89size tc89decl tc89qual tc89comp tc89swjt tc89bit ^
             tc89pp tc89fnty tc89flt tc89fltc tc89flta tc89fptr tc89fs tc89fcmp ^
             tc89fcnv tc89fadd tc89fmul tc89fdiv tc89ffio tc89flng tc89fmat ^
             ttrig tlog tphi tap cpmenumd tbits tfo pihex tstrify tlcont primes ^
             tpreproc trwold tlimits spsmash tcrcfix trtl2 tsyntax tstr2 tstr3 ^
             tlongaud tlongreg tppreg tinitreg ttypesr ttype2 tdecinit tmalloch ^
             tpostut tbug2 tlongsub treg tret tstructv tstructi tstructp tstri2 ^
             tunion2 tbitfld tcnstfld tpromo tkandr tc89ini2 tdecl tctype ^
             tptrdiff tmulpow2 toffset tc89fini tmod3216 tpromo2 tunaryp

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

for %%a in (%_applist%) do (
    echo test %%a
    echo test %%a>>"%outfile%"

    if exist %%a.com del %%a.com

    call ma %%a %peepmode% >nul
    if errorlevel 1 exit /b 1

    if "%%a"=="ttt" (
        %_emulator% %%a 10 >>"%outfile%"
    ) else if "%%a"=="wumpus" (
        %_emulator% %%a -c >>"%outfile%"
    ) else if "%%a"=="tchess" (
        %_emulator% %%a -c >>"%outfile%"
    ) else if "%%a"=="targs" (
        %_emulator% %%a a bb ccc dddd eeeee >>"%outfile%"
    ) else (
        %_emulator% %%a >>"%outfile%"
    )

    if exist %%a.mac del %%a.mac
    if exist %%a.prn del %%a.prn
    if exist %%a.rel del %%a.rel
    if exist %%a.com del %%a.com
)
exit /b
