@echo off
setlocal
rem Build HelicopterOptions.asi as Win32/x86 for NFSMW Most Wanted (2005) v1.3.
rem Edit this path if your Visual Studio install is somewhere else.
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x86 -host_arch=x64
if errorlevel 1 goto :err

cl /nologo /EHsc /O2 /W4 /DNDEBUG /DWIN32 /std:c++17 /LD ^
  HelicopterOptions\dllmain.cpp ^
  HelicopterOptions\src\Config\Ini.cpp ^
  HelicopterOptions\src\Core\Detour.cpp ^
  HelicopterOptions\src\Core\ExeIdentity.cpp ^
  HelicopterOptions\src\Core\FrameTime.cpp ^
  HelicopterOptions\src\Core\Log.cpp ^
  HelicopterOptions\src\Core\Memory.cpp ^
  HelicopterOptions\src\Core\PatchManager.cpp ^
  HelicopterOptions\src\Game\AIActionHeliExit.cpp ^
  HelicopterOptions\src\Game\AIActionHeliPursuit.cpp ^
  HelicopterOptions\src\Game\AICopManager.cpp ^
  HelicopterOptions\src\Game\AIPerpVehicle.cpp ^
  HelicopterOptions\src\Game\AIVehicleHelicopter.cpp ^
  HelicopterOptions\src\Game\HeliSheet.cpp ^
  HelicopterOptions\src\Game\Interfaces.cpp ^
  HelicopterOptions\src\Game\SimpleChopper.cpp ^
  /Fe:HelicopterOptions.asi user32.lib kernel32.lib
if errorlevel 1 goto :err

del *.obj >nul 2>&1
if exist HelicopterOptions.lib del HelicopterOptions.lib
if exist HelicopterOptions.exp del HelicopterOptions.exp

echo Built HelicopterOptions.asi
exit /b 0

:err
echo Build failed.
exit /b 1
