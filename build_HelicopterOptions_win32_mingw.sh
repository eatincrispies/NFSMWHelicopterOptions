#!/usr/bin/env sh
set -e

# Build HelicopterOptions.asi as a Win32/x86 ASI for NFSMW 2005.
# Make sure i686-w64-mingw32-g++ is in your path or edit this path
if [ "$CPP" = "" ]; then CPP="i686-w64-mingw32-g++"; fi

$CPP -shared -o HelicopterOptions.asi -march=i486 -D_WIN32_WINNT=0x0400 -DWINVER=0x0400 -O2 \
		HelicopterOptions/pseh/pseh.c \
		HelicopterOptions/dllmain.cpp \
		HelicopterOptions/src/Core/Log.cpp \
		HelicopterOptions/src/Core/Memory.cpp \
		HelicopterOptions/src/Core/ExeIdentity.cpp \
		HelicopterOptions/src/Core/PatchManager.cpp \
		HelicopterOptions/src/Core/HookManager.cpp \
		HelicopterOptions/src/Config/Ini.cpp \
		HelicopterOptions/src/Config/Validation.cpp \
		HelicopterOptions/src/Game/HeliState.cpp \
		HelicopterOptions/src/Systems/AiCore.cpp \
		HelicopterOptions/src/Systems/HelicopterRegistry.cpp \
		HelicopterOptions/src/Systems/SkidAttack.cpp \
		HelicopterOptions/src/Systems/Movement.cpp \
		HelicopterOptions/src/Systems/SpeedRegulator.cpp \
		HelicopterOptions/src/Systems/Vision.cpp \
		HelicopterOptions/src/Systems/ExitBehavior.cpp \
		HelicopterOptions/src/Systems/HeliSheetControl.cpp \
		HelicopterOptions/src/Systems/Spawner.cpp \
		HelicopterOptions/src/Systems/Telemetry.cpp \
	-static-libgcc -static-libstdc++
