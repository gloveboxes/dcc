@echo off
setlocal

where cl >nul 2>nul
if errorlevel 1 (
	set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
	if exist "%VSWHERE%" (
		for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find VC\Auxiliary\Build\vcvars64.bat`) do (
			call "%%I"
		)
	)
)

where cl >nul 2>nul
if errorlevel 1 (
	for %%Y in (2022 2019) do (
		for %%E in (BuildTools Community Professional Enterprise) do (
			if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\%%Y\%%E\VC\Auxiliary\Build\vcvars64.bat" (
				call "%ProgramFiles(x86)%\Microsoft Visual Studio\%%Y\%%E\VC\Auxiliary\Build\vcvars64.bat"
			)
		)
	)
)

where cl >nul 2>nul
if errorlevel 1 (
	echo cl.exe is not available. Run from a Visual Studio Developer Command Prompt or install the Visual Studio C++ build tools. 1>&2
	exit /b 1
)

if exist dcc del /q dcc
if exist dcc.exe del /q dcc.exe
if exist dccpeep del /q dccpeep
if exist dccpeep.exe del /q dccpeep.exe
if exist dccrtlstrip del /q dccrtlstrip
if exist dccrtlstrip.exe del /q dccrtlstrip.exe
if exist dccmake del /q dccmake
if exist dccmake.exe del /q dccmake.exe
if exist m80c del /q m80c
if exist m80c.exe del /q m80c.exe
if exist l80c del /q l80c
if exist l80c.exe del /q l80c.exe

pushd src\dcc
call build-dcc.bat
popd

rem cl /nologo dcc.c /GS- /GL /Oti2 /Ob3 /Qpar /Fa /FAsc /EHac /Zi /D_AMD64_ /link user32.lib ntdll.lib /OPT:REF

cl /nologo src\dccpeep\*.c /I src\dccpeep /GS- /GL /Oti2 /Ob3 /Qpar /Fa /FAsc /EHac /Zi /std:c11 /D_AMD64_ /link user32.lib ntdll.lib /OPT:REF

cl /nologo src\dccrtlstrip\dccrtlstrip.c /GS- /GL /Oti2 /Ob3 /Qpar /Fa /FAsc /EHac /Zi /D_AMD64_ /link user32.lib ntdll.lib /OPT:REF

cl /nologo src\dccmake\dccmake.c /GS- /GL /Oti2 /Ob3 /Qpar /Fa /FAsc /EHac /Zi /D_AMD64_ /link user32.lib ntdll.lib /OPT:REF

cl /nologo src\m80c\m80c.c /GS- /GL /Oti2 /Ob3 /Qpar /Fa /FAsc /EHac /Zi /D_AMD64_ /link user32.lib ntdll.lib /OPT:REF

cl /nologo src\l80c\l80c.c /GS- /GL /Oti2 /Ob3 /Qpar /Fa /FAsc /EHac /Zi /D_AMD64_ /link user32.lib ntdll.lib /OPT:REF

