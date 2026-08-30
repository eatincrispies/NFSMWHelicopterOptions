// HeliSheetControl.cpp - HeliSheet terrain-safety patches.
//
// The optional per-tick "safe override" (allowing the helicopter to descend
// when it parks too high over the player) is applied at runtime in AiCore.
// This file only handles the one static patch:
//  * RespectHeliSheetDuringSkid keeps the terrain sheet respected during
//    attacks. Value-guarded and journaled; restored on unload.
#include "Systems.h"
#include "../Config/Config.h"
#include "../Core/Addresses.h"
#include "../Core/PatchManager.h"
#include "../Core/Log.h"

namespace Systems {

    void ApplyHeliSheet() {
        if (gCfg.RespectHeliSheetDuringSkid) {
            Patch::Begin("HeliSheet::RespectDuringSkid");
            Patch::AddDataBool("NeverIgnoreHeliSheet := 1", Addr::kNeverIgnoreHeliSheet, 1);
            Patch::Commit();
        }
    }

} // namespace Systems
