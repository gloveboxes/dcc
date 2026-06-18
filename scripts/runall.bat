@echo off
rem Compatibility launcher for the Python dcc regression runner.
setlocal
set SCRIPT_DIR=%~dp0
set PYTHON_CMD=

where python >nul 2>nul
if not errorlevel 1 set PYTHON_CMD=python

if not defined PYTHON_CMD (
	where py >nul 2>nul
	if not errorlevel 1 set PYTHON_CMD=py -3
)

where python3 >nul 2>nul
if not defined PYTHON_CMD if not errorlevel 1 set PYTHON_CMD=python3

if not defined PYTHON_CMD (
	echo Python 3 is required to run scripts\runall.py.
	echo.
	echo Please download and install Python from:
	echo   https://www.python.org/downloads/windows/
	echo.
	set /p OPEN_PYTHON=Open the Python download page now? [Y/N] 
	if /I "%OPEN_PYTHON%"=="Y" start "" "https://www.python.org/downloads/windows/"
	exit /b 1
)

%PYTHON_CMD% "%SCRIPT_DIR%runall.py" %*
exit /b %ERRORLEVEL%
