// NFSMW HelicopterOptions - police helicopter overhaul for
// Need for Speed: Most Wanted (2005). Entry point; features live in src/.
// Supported game: NFSMW PC v1.3 (English). Configuration files live in
// scripts\HelicopterOptions\Configuration\.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "src/Core/Log.h"
#include "src/Core/Version.h"
#include "src/Core/ExeIdentity.h"
#include "src/Core/FrameTime.h"
#include "src/Core/PatchManager.h"
#include "src/Core/HookManager.h"
#include "src/Config/Config.h"
#include "src/Config/Ini.h"
#include "src/Game/HeliState.h"
#include "src/Systems/Systems.h"
#include "src/Systems/AiCore.h"
#include "src/Systems/HelicopterRegistry.h"
#include "src/Radio/HeliRadioChat.h"

namespace {

    HMODULE gModule = nullptr;

    DWORD WINAPI InitThread(void*) {
        Sleep(1000);

        Log::Open(gModule);
        Log::Info("NFSMW HelicopterOptions " HO_VERSION_STR " starting.");

        Ini::LoadConfig(gModule);

        const ExeIdentity::Result id = ExeIdentity::Validate();
        if (id != ExeIdentity::kSupported) {
            if (gCfg.AllowUnsupportedExe && id == ExeIdentity::kUnsupported) {
                Log::Warn("This does not look like the supported game version, but "
                          "AllowUnsupportedExe is enabled. Each change is still checked "
                          "for safety before it is applied, but this setup is untested.");
            } else {
                Log::Error("Unsupported game version detected. NFSMW HelicopterOptions "
                           "supports the English PC v1.3 executable listed in README.md. "
                           "No game files were modified. To try anyway, set "
                           "AllowUnsupportedExe = 1 in GeneralSettings.ini.");
                Log::Close();
                return 0;
            }
        }

        FrameTime::Init();
        Systems::HeliRadioChat::Initialize();

        Systems::ApplySkidEntry();
        Systems::ApplyLead();
        Systems::ApplyAltitude();
        Systems::ApplySkidStrike();
        Systems::ApplyAcceleration();
        Systems::ApplySteering();
        Systems::ApplySmoothing();
        Systems::ApplyVision();
        Systems::ApplyExitBehavior();
        Systems::ApplyHeliSheet();
        Systems::ApplySpawner();
        Systems::ApplyHighFpsFix();

        if (Systems::AnyTickConsumerEnabled()) {
            Hook::RegisterTick(&Systems::AiTick);
            Hook::RegisterTick(&Systems::SpeedRegulatorTick);
            Hook::RegisterTick(&Systems::TelemetryTick);
            Hook::InstallOnDrivingHook();
            // Once-per-spawn: records the helicopter's pursuit action so its
            // live chase/search/attack state can be read.
            Hook::InstallHeliActionCtorHook(&HeliState::NoteHeliAction);
        }
        if (Systems::Registry::Enabled())
            Hook::InstallHeliCtorHook(&Systems::Registry::OnCtor);

        Systems::LogActiveSystems();

        if (Patch::FailedCount() == 0 && Patch::SkippedGroupCount() == 0) {
            Log::Info("Initialization completed successfully.");
        } else if (Patch::FailedCount() == 0) {
            Log::Warn("Initialization completed, but some options were skipped because "
                      "another mod has already changed the same game code. Those options "
                      "are inactive; everything else is working normally.");
        } else {
            Log::Warn("Initialization completed with problems. If the helicopter behaves "
                      "unexpectedly, see the troubleshooting section in README.md.");
        }
        return 0;
    }

} // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        gModule = hModule;
        DisableThreadLibraryCalls(hModule);
        if (HANDLE t = CreateThread(nullptr, 0, InitThread, nullptr, 0, nullptr))
            CloseHandle(t);
        break;

    case DLL_PROCESS_DETACH:
        Systems::HeliRadioChat::Shutdown();
        if (reserved == nullptr) {
            Hook::Remove();
            Patch::RestoreAll();
        }
        Log::Close();
        break;
    }
    return TRUE;
}
