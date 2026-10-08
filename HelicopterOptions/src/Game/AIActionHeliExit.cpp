#include "AIActionHeliExit.h"
#include "Interfaces.h"
#include "../Config/Config.h"

namespace AIActionHeliExit {

    namespace {

        constexpr Patch::FloatPush kFlySpeed = { 0x00427BFEu, 100.0f };

        bool  gPatched = false;
        float gFlySpeed = 0.0f;

    }

    void InstallPatches() {
        Patch::Begin("AIActionHeliExit::Update");
        Patch::PushFloat("FlySpeed", kFlySpeed, gCfg.FlySpeed);
        gPatched = Patch::Commit();
        gFlySpeed = gCfg.FlySpeed;
    }

    void Refresh() {
        if (!gPatched || gCfg.FlySpeed == gFlySpeed) return;
        if (!Patch::RewritePushedFloat(kFlySpeed, gFlySpeed, gCfg.FlySpeed)) {
            gPatched = false;
            Log::Warn("Another mod changed AIActionHeliExit::Update, so [Helicopter:FlySpeed] stops updating.");
            return;
        }
        gFlySpeed = gCfg.FlySpeed;
    }

}
