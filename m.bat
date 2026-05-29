@echo off
setlocal

rem call "c:\program files\microsoft visual studio\18\community\vc\auxiliary\build\vcvars64.bat"
call "c:\program files\microsoft visual studio\2022\community\vc\auxiliary\build\vcvars64.bat"

cl /nologo dcc.c /GS- /GL /Oti2 /Ob3 /Qpar /Fa /FAsc /EHac /Zi /D_AMD64_ /link user32.lib ntdll.lib /OPT:REF

cl /nologo dccpeep.c /GS- /GL /Oti2 /Ob3 /Qpar /Fa /FAsc /EHac /Zi /D_AMD64_ /link user32.lib ntdll.lib /OPT:REF

cl /nologo dccrtlstrip.c /GS- /GL /Oti2 /Ob3 /Qpar /Fa /FAsc /EHac /Zi /D_AMD64_ /link user32.lib ntdll.lib /OPT:REF

