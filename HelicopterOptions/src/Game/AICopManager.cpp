#include "AICopManager.h"
#include "../Config/Config.h"
#include "../Core/Addresses.h"
#include "../Core/Memory.h"
#include "../Core/PatchManager.h"

namespace AICopManager {

    namespace {

        bool  gPatched = false;
        float gSpawnDistance = 0.0f;

    }

    void InstallPatches() {
        Patch::Begin("AICopManager::SpawnPursuitHelicopter");
        Patch::PushFloat("SpawnDistance", Addr::AICopManager::SpawnDistance, gCfg.SpawnDistance);
        gPatched = Patch::Commit();
        gSpawnDistance = gCfg.SpawnDistance;
    }

    void Refresh() {
        if (!gPatched || gCfg.SpawnDistance == gSpawnDistance) return;
        Memory::WriteCode(Addr::AICopManager::SpawnDistance.va + 1, &gCfg.SpawnDistance, sizeof(float));
        gSpawnDistance = gCfg.SpawnDistance;
    }

}
