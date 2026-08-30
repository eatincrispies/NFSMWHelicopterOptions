// Spawner.cpp - [ChopperSpawner] dispatch/spawn gate patches.
//
// Experimental: these gates can make the game create additional helicopters,
// but the engine only tracks a single helicopter at a time, so extra
// helicopters can corrupt state when they despawn. Off by default; intended
// for experimentation only, paired with lifecycle and proximity logging.
#include "Systems.h"
#include "../Config/Config.h"
#include "../Core/Addresses.h"
#include "../Core/PatchManager.h"
#include "../Core/Log.h"

namespace Systems {

    void ApplySpawner() {
        if (!gCfg.EnableDispatchPatches) {
            Log::Verbose("[ChopperSpawner] disabled.");
            return;
        }
        static const uint8_t kNop2[2] = { 0x90, 0x90 };
        static const uint8_t kNop6[6] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };

        if (gCfg.AllowCopheliWeightedSelection) {
            static const uint8_t kJmpShort0E[2] = { 0xEB, 0x0E };
            Patch::Begin("ChopperSpawner::AllowCopheliWeightedSelection");
            Patch::AddBytes("Keep copheli weighted", Addr::Site::CopheliWeightGate,
                            Addr::Guard::CopheliWeightGate, 2, kJmpShort0E, 2);
            Patch::Commit();
        }
        if (gCfg.IgnoreExistingHeliForSelectorTrigger) {
            Patch::Begin("ChopperSpawner::IgnoreExistingHeliSelector");
            Patch::AddBytes("NOP selector heli-involved gate", Addr::Site::SelectorHeliGate,
                            Addr::Guard::SelectorHeliGate, 2, kNop2, 2);
            Patch::Commit();
        }
        if (gCfg.IgnoreExistingHeliForSpecialSpawn) {
            Patch::Begin("ChopperSpawner::IgnoreExistingHeliSpecialSpawn");
            Patch::AddBytes("NOP special-spawn heli-involved gate", Addr::Site::SpecialSpawnHeliGate,
                            Addr::Guard::SpecialSpawnGate, 6, kNop6, 6);
            Patch::Commit();
        }
        if (gCfg.ForceWeightedSelectorAlwaysReturnCopheli) {
            Patch::Begin("ChopperSpawner::ForceSelectorCopheli");
            Patch::AddBytes("Force copheli return path", Addr::Site::ForceSelectorCopheli,
                            Addr::Guard::ForceSelector, 2, kNop2, 2);
            Patch::Commit();
        }
        if (gCfg.BypassSpawnCapForImmediateRequests) {
            Patch::Begin("ChopperSpawner::BypassSpawnCap (ALL vehicle types)");
            Patch::AddBytes("NOP spawn cap branch", Addr::Site::SpawnCapBranch,
                            Addr::Guard::SpawnCapBranch, 2, kNop2, 2);
            Patch::Commit();
        }
        if (gCfg.LogSpawnEvents)
            Log::Info("[ChopperSpawner] research gates applied as configured; lifecycle "
                      "logging %s.", (gCfg.LogHeliLifecycle ? "active" : "OFF (enable "
                      "[Telemetry] LogHeliLifecycle for useful research data)"));
    }

} // namespace Systems
