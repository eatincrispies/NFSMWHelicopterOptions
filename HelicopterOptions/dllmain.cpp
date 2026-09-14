#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "src/Config/Ini.h"
#include "src/Core/Detour.h"
#include "src/Core/ExeIdentity.h"
#include "src/Core/FrameTime.h"
#include "src/Core/Log.h"
#include "src/Core/PatchManager.h"
#include "src/Core/Version.h"
#include "src/Game/AIActionHeliExit.h"
#include "src/Game/AIActionHeliPursuit.h"
#include "src/Game/AICopManager.h"
#include "src/Game/AIPerpVehicle.h"
#include "src/Game/AIVehicleHelicopter.h"
#include "src/Game/SimpleChopper.h"

namespace {

    HMODULE gModule = nullptr;

    DWORD WINAPI Initialize(void*) {
        Sleep(1000);

        Log::Open(gModule);
        Log::Info("NFSMW HelicopterOptions " HO_VERSION_STR " starting.");

        if (!ExeIdentity::IsSupported()) {
            Log::Close();
            return 0;
        }

        Ini::Load(gModule);
        FrameTime::Init();

        AIActionHeliPursuit::InstallPatches();
        SimpleChopper::InstallPatches();
        AIActionHeliExit::InstallPatches();
        AICopManager::InstallPatches();

        AIPerpVehicle::HookSetHeat();
        AIActionHeliPursuit::HookConstructor();
        AIVehicleHelicopter::HookOnDriving();

        const int problems = Patch::SkippedGroups() + Patch::FailedGroups();
        if (problems == 0)
            Log::Info("Ready.");
        else
            Log::Warn("Ready, but %d patch group(s) are inactive; see the lines above.", problems);
        return 0;
    }

}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        gModule = module;
        DisableThreadLibraryCalls(module);
        if (HANDLE thread = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr))
            CloseHandle(thread);
        break;

    case DLL_PROCESS_DETACH:
        if (reserved == nullptr) {
            Detour::RemoveAll();
            SimpleChopper::RestoreSpeedCap();
            Patch::RestoreAll();
        }
        Log::Close();
        break;
    }
    return TRUE;
}
