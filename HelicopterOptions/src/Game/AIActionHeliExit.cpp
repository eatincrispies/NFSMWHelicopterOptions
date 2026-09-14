#include "AIActionHeliExit.h"
#include "../Config/Config.h"
#include "../Core/Addresses.h"
#include "../Core/Memory.h"
#include "../Core/PatchManager.h"

namespace AIActionHeliExit {

    namespace {

        bool  gPatched = false;
        float gFlySpeed = 0.0f;

    }

    void InstallPatches() {
        Patch::Begin("AIActionHeliExit::Update");
        Patch::PushFloat("FlySpeed", Addr::AIActionHeliExit::FlySpeed, gCfg.FlySpeed);
        gPatched = Patch::Commit();
        gFlySpeed = gCfg.FlySpeed;
    }

    void Refresh() {
        if (!gPatched || gCfg.FlySpeed == gFlySpeed) return;
        Memory::WriteCode(Addr::AIActionHeliExit::FlySpeed.va + 1, &gCfg.FlySpeed, sizeof(float));
        gFlySpeed = gCfg.FlySpeed;
    }

}
