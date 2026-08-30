// Vision.cpp - IsPerpInSight branch control in AIActionHeliPursuit::Update.
// Persistent pursuit vision: the branch is forced so the helicopter never
// drops into search/blue mode while alive. Site in a dump export gap but
// exe-verified and corroborated by EA source.
//
// Grace periods, target memory, reacquisition shaping and occlusion queries
// are NOT implemented: the search internals live in unreachable action-
// object state and no world visibility query is verified. Documented in
// KNOWN_LIMITATIONS.md - no fake keys.
#include "Systems.h"
#include "../Config/Config.h"
#include "../Core/Addresses.h"
#include "../Core/PatchManager.h"
#include "../Core/Log.h"

namespace Systems {

    void ApplyVision() {
        if (!gCfg.EnableVisionPatches) { Log::Verbose("[ChopperVision] disabled."); return; }
        if (!gCfg.SeeThroughWalls) {
            Log::Info("[ChopperVision] enabled but SeeThroughWalls=0; nothing to patch.");
            return;
        }
        Patch::Begin("ChopperVision::SeeThroughWalls");
        Patch::AddImm8("Force IsPerpInSight branch", Addr::Site::VisionSightBranch,
                       Addr::Guard::VisionBranch, 4, Addr::Site::VisionSightJnz, 0xEB);
        Patch::Commit();
    }

} // namespace Systems
