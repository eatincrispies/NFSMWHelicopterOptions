#include "AICopManager.h"
#include "Interfaces.h"
#include "../Config/Config.h"

namespace AICopManager {

    namespace {

        constexpr Patch::FloatPush kSpawnDistance = { 0x00426ABFu, 250.0f };

        bool  gPatched = false;
        float gSpawnDistance = 0.0f;

    }

    void InstallPatches() {
        Patch::Begin("AICopManager::SpawnPursuitHelicopter");
        Patch::PushFloat("SpawnDistance", kSpawnDistance, gCfg.SpawnDistance);
        gPatched = Patch::Commit();
        gSpawnDistance = gCfg.SpawnDistance;
    }

    void Refresh() {
        if (!gPatched || gCfg.SpawnDistance == gSpawnDistance) return;
        if (!Patch::RewritePushedFloat(kSpawnDistance, gSpawnDistance, gCfg.SpawnDistance)) {
            gPatched = false;
            Log::Warn("Another mod changed AICopManager::SpawnPursuitHelicopter, so [Helicopter:SpawnDistance] stops updating.");
            return;
        }
        gSpawnDistance = gCfg.SpawnDistance;
    }

}
