@echo off
setlocal

rem Build HelicopterOptions.asi as a Win32/x86 ASI for NFSMW 2005.
rem Edit this path if your Visual Studio install is somewhere else.
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64
if errorlevel 1 goto :err

cl /nologo /EHsc /O2 /DNDEBUG /LD HelicopterOptions\dllmain.cpp /Fe:HelicopterOptions.asi user32.lib kernel32.lib
if errorlevel 1 goto :err

if exist HelicopterOptions.lib del HelicopterOptions.lib
if exist HelicopterOptions.exp del HelicopterOptions.exp
if exist dllmain.obj del dllmain.obj

echo Built HelicopterOptions.asi
exit /b 0

:err
echo Build failed.
exit /b 1
