#!/usr/bin/env sh
set -e

# Build HelicopterOptions.asi as a Win32/x86 ASI for NFSMW 2005.
# Make sure i686-w64-mingw32-g++ is in your path or edit this path
if [ "$CPP" = "" ]; then CPP="i686-w64-mingw32-g++"; fi

$CPP -shared -o HelicopterOptions.asi -march=i486 -D_WIN32_WINNT=0x0400 -DWINVER=0x0400 -O2 -std=c++17 \
		HelicopterOptions/pseh/pseh.c \
		HelicopterOptions/dllmain.cpp \
		HelicopterOptions/src/Config/Ini.cpp \
		HelicopterOptions/src/Core/Detour.cpp \
		HelicopterOptions/src/Core/ExeIdentity.cpp \
		HelicopterOptions/src/Core/FrameTime.cpp \
		HelicopterOptions/src/Core/Log.cpp \
		HelicopterOptions/src/Core/Memory.cpp \
		HelicopterOptions/src/Core/PatchManager.cpp \
		HelicopterOptions/src/Game/AIActionHeliExit.cpp \
		HelicopterOptions/src/Game/AIActionHeliPursuit.cpp \
		HelicopterOptions/src/Game/AICopManager.cpp \
		HelicopterOptions/src/Game/AIPerpVehicle.cpp \
		HelicopterOptions/src/Game/AIVehicleHelicopter.cpp \
		HelicopterOptions/src/Game/HeliSheet.cpp \
		HelicopterOptions/src/Game/Interfaces.cpp \
		HelicopterOptions/src/Game/SimpleChopper.cpp \
	-static-libgcc -static-libstdc++
