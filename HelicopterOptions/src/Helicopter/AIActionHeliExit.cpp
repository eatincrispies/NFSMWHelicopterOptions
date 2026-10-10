#include "AIActionHeliExit.hpp"

void AIActionHeliExit::Refresh() {
    if (!sPatched || sSettings.FlySpeed == sFlySpeed) return;
    if (!Patch::RewritePushedFloat("AIActionHeliExit::Update | flySpeed", kUpdateFlySpeed, sFlySpeed, sSettings.FlySpeed)) {
        sPatched = false;
        return;
    }
    sFlySpeed = sSettings.FlySpeed;
}
