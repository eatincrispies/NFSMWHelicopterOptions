#include <cstdint>
#include "AICopManager.hpp"
#include "AIPerpVehicle.hpp"

namespace {

    constexpr Patch::FloatPush kSpawnPursuitHelicopterIncNavPosition = { 0x00426ABFu, 250.0f };

    bool  gPatched = false;
    float gSpawnDistance = 0.0f;

}

void AICopManager::InstallPatches() {
    Patch::Begin("AICopManager::SpawnPursuitHelicopter");
    Patch::PushFloat("testNav.IncNavPosition distance", kSpawnPursuitHelicopterIncNavPosition, gCfg.SpawnDistance);
    gPatched = Patch::Commit();
    gSpawnDistance = gCfg.SpawnDistance;
}

void AICopManager::Refresh() {
    if (!gPatched || gCfg.SpawnDistance == gSpawnDistance) return;
    if (!Patch::RewritePushedFloat(kSpawnPursuitHelicopterIncNavPosition, gSpawnDistance, gCfg.SpawnDistance)) {
        gPatched = false;
        Log::Warn("Another mod changed AICopManager::SpawnPursuitHelicopter, so [Helicopter:SpawnDistance] stops updating.");
        return;
    }
    gSpawnDistance = gCfg.SpawnDistance;
}
