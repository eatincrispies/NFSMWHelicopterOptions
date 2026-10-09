#include <cstdint>
#include "AIActionHeliExit.hpp"
#include "Hooks.hpp"

namespace {

    constexpr Patch::FloatPush kUpdateFlySpeed = { 0x00427BFEu, 100.0f };

    bool  gPatched = false;
    float gFlySpeed = 0.0f;

}

void AIActionHeliExit::InstallPatches() {
    Patch::Begin("AIActionHeliExit::Update");
    Patch::PushFloat("flySpeed", kUpdateFlySpeed, gCfg.FlySpeed);
    gPatched = Patch::Commit();
    gFlySpeed = gCfg.FlySpeed;
}

void AIActionHeliExit::Refresh() {
    if (!gPatched || gCfg.FlySpeed == gFlySpeed) return;
    if (!Patch::RewritePushedFloat(kUpdateFlySpeed, gFlySpeed, gCfg.FlySpeed)) {
        gPatched = false;
        Log::Warn("Another mod changed AIActionHeliExit::Update, so [Helicopter:FlySpeed] stops updating.");
        return;
    }
    gFlySpeed = gCfg.FlySpeed;
}
