#include "AIActionHeliExit.hpp"

void AIActionHeliExit::Refresh() {
    if (!sPatched || sSettings.FlySpeed == sFlySpeed) return;
    if (!Patch::RewritePushedFloat(kUpdateFlySpeed, sFlySpeed, sSettings.FlySpeed)) {
        sPatched = false;
        Log::Warn("Another mod changed AIActionHeliExit::Update, so [Helicopter:FlySpeed] stops updating.");
        return;
    }
    sFlySpeed = sSettings.FlySpeed;
}
