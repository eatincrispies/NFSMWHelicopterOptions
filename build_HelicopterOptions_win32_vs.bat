@echo off
setlocal
rem Build NFSMWHelicopterOptions.asi as Win32/x86 for NFSMW Most Wanted (2005) v1.3.
rem Edit this path if your Visual Studio install is somewhere else.
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64
if errorlevel 1 goto :err

cl /nologo /EHsc /O2 /W4 /DNDEBUG /DWIN32 /std:c++17 /LD ^
  HelicopterOptions\dllmain.cpp ^
  HelicopterOptions\src\Helicopter\AIActionHeliExit.cpp ^
  HelicopterOptions\src\Helicopter\AIActionHeliPursuit.cpp ^
  HelicopterOptions\src\Helicopter\AICopManager.cpp ^
  HelicopterOptions\src\Helicopter\AIPerpVehicle.cpp ^
  HelicopterOptions\src\Helicopter\AIVehicleHelicopter.cpp ^
  HelicopterOptions\src\Helicopter\HeliSheet.cpp ^
  HelicopterOptions\src\Helicopter\Hooks.cpp ^
  HelicopterOptions\src\Helicopter\Interfaces.cpp ^
  HelicopterOptions\src\Helicopter\SimpleChopper.cpp ^
  HelicopterOptions\src\Helicopter\SoundAI.cpp ^
  /Fe:NFSMWHelicopterOptions.asi user32.lib kernel32.lib
if errorlevel 1 goto :err

del *.obj >nul 2>&1
if exist NFSMWHelicopterOptions.lib del NFSMWHelicopterOptions.lib
if exist NFSMWHelicopterOptions.exp del NFSMWHelicopterOptions.exp

echo Built NFSMWHelicopterOptions.asi
exit /b 0

:err
echo Build failed.
exit /b 1
