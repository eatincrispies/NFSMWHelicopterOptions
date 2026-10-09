#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "src/Helicopter/AIActionHeliExit.hpp"
#include "src/Helicopter/AIActionHeliPursuit.hpp"
#include "src/Helicopter/AICopManager.hpp"
#include "src/Helicopter/AIPerpVehicle.hpp"
#include "src/Helicopter/AIVehicleHelicopter.hpp"
#include "src/Helicopter/Hooks.hpp"
#include "src/Helicopter/SimpleChopper.hpp"

namespace {

    constexpr const char* kVersion = "V3.3.0";

    HMODULE gModule = nullptr;

    DWORD WINAPI Initialize(void*) {
        Sleep(1000);

        Log::Open(gModule);
        Log::Info("NFSMWHelicopterOptions %s starting.", kVersion);

        Ini::Load(gModule);

        AIActionHeliPursuit::InstallPatches();
        SimpleChopper::InstallPatches();
        AIActionHeliExit::InstallPatches();
        AICopManager::InstallPatches();

        AIPerpVehicle::HookSetHeat();
        AIActionHeliPursuit::HookConstructor();
        AIVehicleHelicopter::HookOnDriving();
        AIVehicleHelicopter::HookCanSeeTarget();

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
        if (HANDLE thread = CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr)) CloseHandle(thread);
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
